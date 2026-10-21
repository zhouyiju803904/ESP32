"""
ESP32 HMI 串口通讯协议层

帧格式:
  [0xAA] [0x55] [TYPE] [LEN_LO] [LEN_HI] [DATA ...] [CHECKSUM]

帧类型:
  0x01 CMD  上位机 → ESP32 命令
  0x02 ACK  ESP32 → 上位机 响应
  0x03 RPT  ESP32 → 上位机 主动上报

校验: TYPE XOR DATA[0] XOR DATA[1] ... XOR DATA[N-1]
"""

import json
import struct

HEADER = b"\xAA\x55"
TYPE_CMD = 0x01
TYPE_ACK = 0x02
TYPE_RPT = 0x03

TYPE_NAMES = {TYPE_CMD: "CMD", TYPE_ACK: "ACK", TYPE_RPT: "RPT"}


def calc_checksum(frame_type: int, data: bytes) -> int:
    """从帧类型到数据末尾的 XOR 累加"""
    checksum = frame_type
    for b in data:
        checksum ^= b
    return checksum


def build_frame(frame_type: int, payload: dict) -> bytes:
    """构建完整帧"""
    data = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    frame = HEADER + bytes([frame_type]) + struct.pack("<H", len(data)) + data
    frame += bytes([calc_checksum(frame_type, data)])
    return frame


def build_cmd(payload: dict) -> bytes:
    """构建 CMD 帧"""
    return build_frame(TYPE_CMD, payload)


def build_state(**fields) -> bytes:
    """构建 STATE 命令帧"""
    payload = {"cmd": "STATE"}
    payload.update(fields)
    return build_cmd(payload)


def build_page(page: str) -> bytes:
    """构建页面切换命令帧"""
    return build_cmd({"cmd": "PAGE", "val": page})


class FrameParser:
    """字节流帧解析器，逐字节喂入，解析出完整帧后回调"""

    def __init__(self):
        self._state = 0  # 0=header0, 1=header1, 2=type, 3=len_lo, 4=len_hi, 5=data, 6=checksum
        self._rx_type = 0
        self._rx_len = 0
        self._rx_idx = 0
        self._rx_data = bytearray()
        self._rx_checksum = 0

    def feed(self, byte_val: int):
        """喂入一个字节，返回 (frame_type, payload_dict) 或 None"""
        if self._state == 0:
            if byte_val == 0xAA:
                self._state = 1
            return None

        if self._state == 1:
            if byte_val == 0x55:
                self._state = 2
            else:
                self._state = 0
            return None

        if self._state == 2:
            if byte_val in (TYPE_CMD, TYPE_ACK, TYPE_RPT):
                self._rx_type = byte_val
                self._rx_checksum = byte_val
                self._state = 3
            else:
                self._state = 0
            return None

        if self._state == 3:
            self._rx_len = byte_val
            self._state = 4
            return None

        if self._state == 4:
            self._rx_len |= byte_val << 8
            if self._rx_len > 256:
                self._state = 0
            elif self._rx_len == 0:
                self._state = 6
            else:
                self._rx_data = bytearray()
                self._rx_idx = 0
                self._state = 5
            return None

        if self._state == 5:
            self._rx_data.append(byte_val)
            self._rx_checksum ^= byte_val
            self._rx_idx += 1
            if self._rx_idx >= self._rx_len:
                self._state = 6
            return None

        if self._state == 6:
            self._state = 0
            if byte_val == self._rx_checksum:
                try:
                    payload = json.loads(self._rx_data.decode("utf-8"))
                    return self._rx_type, payload
                except (json.JSONDecodeError, UnicodeDecodeError):
                    return None
            return None

        return None
