"""
串口通信工作线程

在独立 QThread 中运行串口读写，通过 Qt 信号与主线程 UI 通信。
"""

import time
from PyQt5.QtCore import QThread, pyqtSignal
import serial
import serial.tools.list_ports

from protocol import (
    FrameParser, TYPE_ACK, TYPE_RPT, TYPE_NAMES,
    build_frame, build_state, build_page, build_cmd, TYPE_CMD,
)


class SerialWorker(QThread):
    """串口读写工作线程"""

    # 信号定义
    connected = pyqtSignal()                         # 连接成功
    disconnected = pyqtSignal()                      # 断开连接
    frame_received = pyqtSignal(int, dict)           # (帧类型, JSON payload)
    error_occurred = pyqtSignal(str)                 # 错误消息
    log_message = pyqtSignal(str, str)               # (方向, 原始hex)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._port = None
        self._baudrate = 115200
        self._running = False
        self._parser = FrameParser()
        self._send_queue = []   # 简单的待发送帧列表

    @property
    def is_connected(self):
        return self._port is not None and self._port.is_open

    def connect_port(self, port_name: str, baudrate: int = 115200):
        """请求连接串口（在主线程调用，启动线程前设置）"""
        self._port_name = port_name
        self._baudrate = baudrate

    def disconnect_port(self):
        """请求断开连接"""
        self._running = False

    def send_frame(self, frame_bytes: bytes):
        """发送一帧（线程安全，放入发送队列）"""
        self._send_queue.append(frame_bytes)
        hex_str = " ".join(f"{b:02X}" for b in frame_bytes[:16])
        self.log_message.emit("QUEUED", f"queue={len(self._send_queue)} {hex_str}...")

    def send_state(self, **fields):
        """发送 STATE 命令"""
        self.send_frame(build_state(**fields))

    def send_page(self, page: str):
        """发送页面切换命令"""
        self.send_frame(build_page(page))

    def send_cmd(self, payload: dict):
        """发送任意 CMD 命令"""
        self.send_frame(build_cmd(payload))

    def run(self):
        """线程主循环"""
        try:
            self._port = serial.Serial(
                self._port_name,
                self._baudrate,
                timeout=0.05,
            )
            self._running = True
            self.connected.emit()
        except Exception as e:
            self.error_occurred.emit(f"无法打开 {self._port_name}: {e}")
            return

        while self._running:
            # ---- 发送 ----
            while self._send_queue:
                frame = self._send_queue.pop(0)
                try:
                    if not self._port.is_open:
                        self.error_occurred.emit("串口已关闭，无法发送")
                        break
                    n = self._port.write(frame)
                    hex_str = " ".join(f"{b:02X}" for b in frame)
                    self.log_message.emit("TX", f"written={n} {hex_str}")
                except Exception as e:
                    self.error_occurred.emit(f"发送失败: {e}")
                    self._running = False
                    break

            # ---- 接收 ----
            try:
                waiting = self._port.in_waiting
                if waiting > 0:
                    data = self._port.read(waiting)
                    hex_str = " ".join(f"{b:02X}" for b in data)
                    self.log_message.emit("RX", hex_str)
                    for byte_val in data:
                        result = self._parser.feed(byte_val)
                        if result is not None:
                            frame_type, payload = result
                            self.frame_received.emit(frame_type, payload)
                else:
                    time.sleep(0.001)
            except Exception as e:
                if self._running:
                    self.error_occurred.emit(f"接收错误: {e}")
                self._running = False
                break

        # ---- 清理 ----
        try:
            if self._port and self._port.is_open:
                self._port.close()
        except Exception:
            pass
        self._port = None
        self.disconnected.emit()


def list_serial_ports() -> list:
    """列出可用串口"""
    ports = serial.tools.list_ports.comports()
    return [(p.device, p.description) for p in sorted(ports)]
