"""
ESP32 HMI 上位机 - PyQt5 主界面
"""

import json
import time
from datetime import datetime
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QPushButton, QComboBox, QLineEdit, QSpinBox,
    QDoubleSpinBox, QTextEdit, QTabWidget, QCheckBox, QStatusBar,
    QMessageBox, QSplitter, QFrame, QScrollArea, QSizePolicy,
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont, QColor, QTextCursor

from serial_worker import SerialWorker, list_serial_ports
from protocol import TYPE_ACK, TYPE_RPT, TYPE_NAMES


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ESP32 HMI 上位机")
        self.setMinimumSize(960, 680)

        self._worker = SerialWorker()
        self._heartbeat_timer = QTimer()
        self._heartbeat_timer.timeout.connect(self._send_heartbeat)
        self._state = {}  # 当前已知的 ESP32 状态
        self._syncing = False  # 防止 ESP32 上报同步时触发自动下发

        self._init_ui()
        self._connect_signals()
        self._refresh_ports()

    # ================================================================
    #  UI 构建
    # ================================================================

    def _init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(6)

        # ---- 顶部: 连接栏 ----
        root.addWidget(self._build_connection_bar())

        # ---- 中部: 左右分栏 ----
        splitter = QSplitter(Qt.Horizontal)
        splitter.addWidget(self._build_left_panel())
        splitter.addWidget(self._build_right_panel())
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        root.addWidget(splitter, 1)

        # ---- 底部: 日志 ----
        root.addWidget(self._build_log_panel(), 1)

        # 状态栏
        self._status_label = QLabel("未连接")
        self.statusBar().addPermanentWidget(self._status_label)

    # ---- 连接栏 ----
    def _build_connection_bar(self) -> QGroupBox:
        box = QGroupBox("串口连接")
        layout = QHBoxLayout(box)
        layout.setContentsMargins(8, 4, 8, 4)

        layout.addWidget(QLabel("端口:"))
        self._port_combo = QComboBox()
        self._port_combo.setMinimumWidth(160)
        layout.addWidget(self._port_combo)

        self._btn_refresh = QPushButton("刷新")
        self._btn_refresh.clicked.connect(self._refresh_ports)
        layout.addWidget(self._btn_refresh)

        layout.addWidget(QLabel("波特率:"))
        self._baud_combo = QComboBox()
        self._baud_combo.addItems(["115200", "921600", "460800", "230400", "57600"])
        layout.addWidget(self._baud_combo)

        self._btn_connect = QPushButton("连接")
        self._btn_connect.setFixedWidth(80)
        self._btn_connect.clicked.connect(self._toggle_connection)
        layout.addWidget(self._btn_connect)

        layout.addSpacing(20)
        self._conn_indicator = QLabel("●")
        self._conn_indicator.setStyleSheet("color: gray; font-size: 18px;")
        layout.addWidget(self._conn_indicator)

        layout.addSpacing(10)
        self._heartbeat_check = QCheckBox("心跳包 (1Hz)")
        self._heartbeat_check.toggled.connect(self._toggle_heartbeat)
        layout.addWidget(self._heartbeat_check)

        layout.addStretch()
        return box

    # ---- 左侧面板: 状态显示 + 控制 ----
    def _build_left_panel(self) -> QTabWidget:
        tabs = QTabWidget()
        tabs.addTab(self._build_status_tab(), "状态控制")
        tabs.addTab(self._build_param_config_tab(), "参数配置")
        tabs.addTab(self._build_command_tab(), "命令发送")
        return tabs

    def _build_status_tab(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        # -- 页面切换 --
        page_group = QGroupBox("页面切换")
        page_layout = QHBoxLayout(page_group)
        page_layout.setContentsMargins(8, 4, 8, 4)
        for name, val in [("状态页 STATUS", "STATUS"), ("参数页 PARAM", "PARAM"), ("操作页 OPER", "OPER")]:
            btn = QPushButton(name)
            btn.clicked.connect(lambda checked, v=val: self._send_page(v))
            page_layout.addWidget(btn)
        layout.addWidget(page_group)

        # -- 速度 (上位机下发实际速度, 三个页面均显示) --
        speed_group = QGroupBox("速度")
        sg = QGridLayout(speed_group)
        sg.setContentsMargins(8, 4, 8, 4)
        sg.setHorizontalSpacing(12)

        sg.addWidget(QLabel("当前速度:"), 0, 0)
        self._sp_current = QDoubleSpinBox()
        self._sp_current.setRange(0, 240)
        self._sp_current.setDecimals(1)
        self._sp_current.setValue(0.0)
        sg.addWidget(self._sp_current, 0, 1)

        layout.addWidget(speed_group)

        # -- 坐标 (对应 ESP32 POSITION: X, Y, Theta) --
        pos_group = QGroupBox("坐标 (POSITION)")
        pg = QGridLayout(pos_group)
        pg.setContentsMargins(8, 4, 8, 4)
        pg.setHorizontalSpacing(12)
        pg.setVerticalSpacing(4)

        pg.addWidget(QLabel("X (mm):"), 0, 0)
        self._sp_x = QDoubleSpinBox()
        self._sp_x.setRange(-99999, 99999)
        self._sp_x.setDecimals(1)
        pg.addWidget(self._sp_x, 0, 1)

        pg.addWidget(QLabel("Y (mm):"), 0, 2)
        self._sp_y = QDoubleSpinBox()
        self._sp_y.setRange(-99999, 99999)
        self._sp_y.setDecimals(1)
        pg.addWidget(self._sp_y, 0, 3)

        pg.addWidget(QLabel("Theta (°):"), 1, 0)
        self._sp_theta = QDoubleSpinBox()
        self._sp_theta.setRange(-360, 360)
        self._sp_theta.setDecimals(1)
        pg.addWidget(self._sp_theta, 1, 1)

        layout.addWidget(pos_group)

        # -- 状态文本 --
        text_group = QGroupBox("状态文本")
        tg = QGridLayout(text_group)
        tg.setContentsMargins(8, 4, 8, 4)
        tg.setHorizontalSpacing(12)
        tg.setVerticalSpacing(4)

        tg.addWidget(QLabel("模式:"), 0, 0)
        self._edit_mode = QLineEdit("AUTO")
        self._edit_mode.setMaxLength(11)
        tg.addWidget(self._edit_mode, 0, 1)

        tg.addWidget(QLabel("动作:"), 0, 2)
        self._edit_action = QLineEdit("IDLE")
        self._edit_action.setMaxLength(15)
        tg.addWidget(self._edit_action, 0, 3)

        tg.addWidget(QLabel("错误:"), 1, 0)
        self._edit_error = QLineEdit("")
        self._edit_error.setPlaceholderText("空=无错误")
        self._edit_error.setMaxLength(15)
        tg.addWidget(self._edit_error, 1, 1)

        layout.addWidget(text_group)

        # -- 电池 --
        bat_group = QGroupBox("电池")
        bg = QHBoxLayout(bat_group)
        bg.setContentsMargins(8, 4, 8, 4)

        bg.addWidget(QLabel("电量 (%):"))
        self._sp_bat = QSpinBox()
        self._sp_bat.setRange(0, 100)
        self._sp_bat.setValue(86)
        bg.addWidget(self._sp_bat)

        layout.addWidget(bat_group)

        # -- 发送按钮 --
        btn_layout = QHBoxLayout()
        btn_send = QPushButton("发送 STATE")
        btn_send.setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 8px;")
        btn_send.clicked.connect(self._send_current_state)
        btn_layout.addWidget(btn_send)

        btn_start = QPushButton("START")
        btn_start.setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;")
        btn_start.clicked.connect(lambda: self._send_quick_cmd("START"))
        btn_layout.addWidget(btn_start)

        btn_stop = QPushButton("STOP")
        btn_stop.setStyleSheet("background-color: #F44336; color: white; padding: 8px;")
        btn_stop.clicked.connect(lambda: self._send_quick_cmd("STOP"))
        btn_layout.addWidget(btn_stop)

        btn_home = QPushButton("HOME")
        btn_home.setStyleSheet("background-color: #FF9800; color: white; padding: 8px;")
        btn_home.clicked.connect(lambda: self._send_quick_cmd("HOME"))
        btn_layout.addWidget(btn_home)

        layout.addLayout(btn_layout)
        layout.addStretch()
        return w

    def _build_param_config_tab(self) -> QWidget:
        """参数配置 tab — 存储 PARAM/OPER 页参数, ESP32 切换页面时自动回复"""
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        # -- PARAM 页参数 (参数页, 现场设置) --
        param_group = QGroupBox("PARAMETER 参数页 (ESP32 切换到此页时自动下发)")
        pg = QGridLayout(param_group)
        pg.setContentsMargins(8, 4, 8, 4)
        pg.setHorizontalSpacing(12)
        pg.setVerticalSpacing(4)

        pg.addWidget(QLabel("目标速度:"), 0, 0)
        self._sp_target = QDoubleSpinBox()
        self._sp_target.setRange(0, 240)
        self._sp_target.setDecimals(1)
        self._sp_target.setValue(100.0)
        pg.addWidget(self._sp_target, 0, 1)

        pg.addWidget(QLabel("速度上限:"), 0, 2)
        self._sp_limit = QDoubleSpinBox()
        self._sp_limit.setRange(1, 240)
        self._sp_limit.setDecimals(1)
        self._sp_limit.setValue(220.0)
        pg.addWidget(self._sp_limit, 0, 3)

        pg.addWidget(QLabel("加速时间:"), 1, 0)
        self._sp_accel = QDoubleSpinBox()
        self._sp_accel.setRange(0.1, 30.0)
        self._sp_accel.setDecimals(1)
        self._sp_accel.setValue(2.5)
        self._sp_accel.setSingleStep(0.5)
        pg.addWidget(self._sp_accel, 1, 1)

        # -- 自动下发: 修改值后立即发送给 ESP32 --
        self._sp_target.valueChanged.connect(self._auto_send_param)
        self._sp_limit.valueChanged.connect(self._auto_send_param)
        self._sp_accel.valueChanged.connect(self._auto_send_param)

        layout.addWidget(param_group)

        # -- OPER 页参数 (操作页, 现场设置) --
        oper_group = QGroupBox("BASIC OPERATION 操作页 (ESP32 切换到此页时自动下发)")
        og = QGridLayout(oper_group)
        og.setContentsMargins(8, 4, 8, 4)
        og.setHorizontalSpacing(12)
        og.setVerticalSpacing(4)

        self._chk_run = QCheckBox("运行中 (RUNNING)")
        og.addWidget(self._chk_run, 0, 0, 1, 2)

        og.addWidget(QLabel("LIFT HEIGHT:"), 1, 0)
        self._edit_lift = QLineEdit("50")
        self._edit_lift.setMaxLength(11)
        og.addWidget(self._edit_lift, 1, 1)

        og.addWidget(QLabel("NAME 前缀:"), 2, 0)
        self._combo_name_prefix = QComboBox()
        self._combo_name_prefix.addItems(["SLAM", "QRCODE"])
        og.addWidget(self._combo_name_prefix, 2, 1)

        og.addWidget(QLabel("NAME 编号:"), 3, 0)
        self._edit_name_num = QLineEdit("001")
        self._edit_name_num.setMaxLength(15)
        og.addWidget(self._edit_name_num, 3, 1)

        # -- 自动下发: 修改值后立即发送给 ESP32 --
        self._chk_run.toggled.connect(self._auto_send_oper)
        self._edit_lift.textChanged.connect(self._auto_send_oper)
        self._combo_name_prefix.currentTextChanged.connect(self._auto_send_oper)
        self._edit_name_num.textChanged.connect(self._auto_send_oper)

        layout.addWidget(oper_group)

        # -- 手动下发按钮 --
        btn_layout = QHBoxLayout()
        btn_param = QPushButton("下发 PARAM 参数")
        btn_param.setStyleSheet("background-color: #2196F3; color: white; padding: 6px;")
        btn_param.clicked.connect(self._respond_param_req)
        btn_layout.addWidget(btn_param)

        btn_oper = QPushButton("下发 OPER 参数")
        btn_oper.setStyleSheet("background-color: #FF9800; color: white; padding: 6px;")
        btn_oper.clicked.connect(self._respond_oper_req)
        btn_layout.addWidget(btn_oper)

        layout.addLayout(btn_layout)
        layout.addStretch()
        return w

    def _build_command_tab(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(6, 6, 6, 6)

        group = QGroupBox("自定义 JSON 命令")
        gl = QVBoxLayout(group)

        self._cmd_edit = QLineEdit()
        self._cmd_edit.setPlaceholderText('例: {"cmd":"SPEED","val":120.0}')
        self._cmd_edit.setFont(QFont("Consolas", 10))
        gl.addWidget(self._cmd_edit)

        btn_send = QPushButton("发送")
        btn_send.clicked.connect(self._send_custom_cmd)
        gl.addWidget(btn_send)

        layout.addWidget(group)

        # 快捷命令
        quick_group = QGroupBox("快捷命令")
        qg = QGridLayout(quick_group)
        qg.setContentsMargins(8, 4, 8, 4)

        quick_cmds = [
            ("QUERY", '{"cmd":"QUERY"}'),
            ("SPEED 50", '{"cmd":"SPEED","val":50.0}'),
            ("SPEED 100", '{"cmd":"SPEED","val":100.0}'),
            ("LIMIT 200", '{"cmd":"LIMIT","val":200.0}'),
            ("ACCEL 3.0", '{"cmd":"ACCEL","val":3.0}'),
            ("X 0", '{"cmd":"X","val":0.0}'),
            ("Y 0", '{"cmd":"Y","val":0.0}'),
        ]
        for i, (label, json_str) in enumerate(quick_cmds):
            row, col = divmod(i, 4)
            btn = QPushButton(label)
            btn.clicked.connect(lambda checked, j=json_str: self._send_raw_json(j))
            qg.addWidget(btn, row, col)

        layout.addWidget(quick_group)
        layout.addStretch()
        return w

    # ---- 右侧面板: ESP32 上报信息 ----
    def _build_right_panel(self) -> QTabWidget:
        tabs = QTabWidget()
        tabs.addTab(self._build_report_panel(), "上报消息")
        tabs.addTab(self._build_state_view(), "状态视图")
        return tabs

    def _build_report_panel(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(6, 6, 6, 6)

        self._report_text = QTextEdit()
        self._report_text.setReadOnly(True)
        self._report_text.setFont(QFont("Consolas", 9))
        self._report_text.setPlaceholderText("等待 ESP32 上报 PARAM / OPER 消息...")
        layout.addWidget(self._report_text)

        btn_clear = QPushButton("清除")
        btn_clear.clicked.connect(self._report_text.clear)
        layout.addWidget(btn_clear)
        return w

    def _build_state_view(self) -> QWidget:
        w = QWidget()
        layout = QVBoxLayout(w)
        layout.setContentsMargins(6, 6, 6, 6)

        self._state_text = QTextEdit()
        self._state_text.setReadOnly(True)
        self._state_text.setFont(QFont("Consolas", 9))
        self._state_text.setPlaceholderText("等待 ACK / 状态数据...")
        layout.addWidget(self._state_text)

        btn_clear = QPushButton("清除")
        btn_clear.clicked.connect(self._state_text.clear)
        layout.addWidget(btn_clear)
        return w

    # ---- 底部日志 ----
    def _build_log_panel(self) -> QGroupBox:
        box = QGroupBox("通讯日志")
        layout = QVBoxLayout(box)
        layout.setContentsMargins(6, 4, 6, 4)

        self._log_text = QTextEdit()
        self._log_text.setReadOnly(True)
        self._log_text.setFont(QFont("Consolas", 8))
        self._log_text.setMaximumHeight(180)
        self._log_text.setPlaceholderText("原始帧数据...")
        layout.addWidget(self._log_text)

        btn_layout = QHBoxLayout()
        btn_clear = QPushButton("清除日志")
        btn_clear.clicked.connect(self._log_text.clear)
        btn_layout.addWidget(btn_clear)

        self._log_check = QCheckBox("显示原始 HEX")
        self._log_check.setChecked(True)
        btn_layout.addWidget(self._log_check)

        btn_layout.addStretch()
        layout.addLayout(btn_layout)
        return box

    # ================================================================
    #  信号连接
    # ================================================================

    def _connect_signals(self):
        self._worker.connected.connect(self._on_connected)
        self._worker.disconnected.connect(self._on_disconnected)
        self._worker.frame_received.connect(self._on_frame_received)
        self._worker.error_occurred.connect(self._on_error)
        self._worker.log_message.connect(self._on_log)

    # ================================================================
    #  串口操作
    # ================================================================

    def _refresh_ports(self):
        self._port_combo.clear()
        ports = list_serial_ports()
        for dev, desc in ports:
            self._port_combo.addItem(f"{dev} - {desc}", dev)

    def _toggle_connection(self):
        if self._worker.isRunning():
            self._worker.disconnect_port()
        else:
            port_data = self._port_combo.currentData()
            if not port_data:
                QMessageBox.warning(self, "错误", "请选择串口")
                return
            baudrate = int(self._baud_combo.currentText())
            self._worker = SerialWorker()
            self._connect_signals()
            self._worker.connect_port(port_data, baudrate)
            self._worker.start()

    def _on_connected(self):
        self._btn_connect.setText("断开")
        self._conn_indicator.setStyleSheet("color: #4CAF50; font-size: 18px;")
        self._status_label.setText(f"已连接: {self._port_combo.currentData()}")
        self._append_log("SYS", "串口已连接")

    def _on_disconnected(self):
        self._btn_connect.setText("连接")
        self._conn_indicator.setStyleSheet("color: gray; font-size: 18px;")
        self._status_label.setText("未连接")
        self._heartbeat_check.setChecked(False)
        self._append_log("SYS", "串口已断开")

    def _on_error(self, msg: str):
        self._append_log("ERR", msg)
        QMessageBox.critical(self, "错误", msg)

    # ================================================================
    #  帧接收处理
    # ================================================================

    def _on_frame_received(self, frame_type: int, payload: dict):
        type_name = TYPE_NAMES.get(frame_type, "???")
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        if frame_type == TYPE_ACK:
            self._handle_ack(payload, ts)
        elif frame_type == TYPE_RPT:
            self._handle_report(payload, ts)

    def _handle_ack(self, payload: dict, ts: str):
        cmd = payload.get("cmd", "?")
        ack = payload.get("ack", "?")
        color = "#4CAF50" if ack == "OK" else "#F44336"
        self._state_text.append(
            f'<span style="color:{color}">[{ts}] ACK {cmd}={ack}</span>'
        )
        self._state_text.append(
            f'<span style="color:#888">{json.dumps(payload, ensure_ascii=False)}</span>'
        )
        # 更新状态视图
        self._state.update(payload)
        self._refresh_state_view()

    def _handle_report(self, payload: dict, ts: str):
        rpt_type = payload.get("rpt", "?")
        if rpt_type == "PARAM_REQ":
            self._report_text.append(
                f'<span style="color:#9C27B0">[{ts}] ESP32 请求 PARAM 参数 → 自动回复</span>'
            )
            self._respond_param_req()
        elif rpt_type == "OPER_REQ":
            self._report_text.append(
                f'<span style="color:#9C27B0">[{ts}] ESP32 请求 OPER 参数 → 自动回复</span>'
            )
            self._respond_oper_req()
        elif rpt_type == "PARAM":
            field = payload.get("field", "?")
            val = payload.get("val", "?")
            self._report_text.append(
                f'<span style="color:#2196F3">[{ts}] PARAM: {field} = {val}</span>'
            )
            self._syncing = True
            self._sync_param_field(field, val)
            self._syncing = False
        elif rpt_type == "OPER":
            cmd = payload.get("cmd", "?")
            height = payload.get("height", "?")
            self._report_text.append(
                f'<span style="color:#FF9800">[{ts}] OPER: {cmd} height={height}</span>'
            )
            if cmd == "LIFT_TO":
                self._syncing = True
                self._edit_lift.setText(str(height))
                self._syncing = False
        else:
            self._report_text.append(
                f'<span style="color:#888">[{ts}] RPT: {json.dumps(payload, ensure_ascii=False)}</span>'
            )

    def _sync_param_field(self, field: str, val):
        """同步 ESP32 上报的参数到参数配置 tab"""
        if field == "spd":
            self._sp_target.setValue(float(val))
        elif field == "limit":
            self._sp_limit.setValue(float(val))
        elif field == "accel":
            self._sp_accel.setValue(float(val))
        elif field == "name":
            name_str = str(val)
            for prefix in ("SLAM-", "QRCODE-"):
                if name_str.startswith(prefix):
                    idx = 0 if prefix == "SLAM-" else 1
                    self._combo_name_prefix.setCurrentIndex(idx)
                    self._edit_name_num.setText(name_str[len(prefix):])
                    break

    # ================================================================
    #  参数请求响应 (ESP32 切换页面时触发)
    # ================================================================

    def _auto_send_param(self):
        """PARAM 参数值变化时自动下发给 ESP32"""
        if self._syncing:
            return
        self._respond_param_req()

    def _auto_send_oper(self):
        """OPER 参数值变化时自动下发给 ESP32"""
        if self._syncing:
            return
        self._respond_oper_req()

    def _respond_param_req(self):
        """回复 PARAM 页参数: 目标速度、速度上限、加速时间"""
        if not self._worker.isRunning():
            return
        fields = {
            "spd": self._sp_target.value(),
            "limit": self._sp_limit.value(),
            "accel": self._sp_accel.value(),
        }
        self._worker.send_state(**fields)
        self._append_log("CMD", f"PARAM_RSP {json.dumps(fields)}")

    def _respond_oper_req(self):
        """回复 OPER 页参数: 运行状态、举升高度、名称"""
        if not self._worker.isRunning():
            return
        name = f"{self._combo_name_prefix.currentText()}-{self._edit_name_num.text()}"
        fields = {
            "run": self._chk_run.isChecked(),
            "lift": float(self._edit_lift.text() or 0),
            "name": name,
        }
        self._worker.send_state(**fields)
        self._append_log("CMD", f"OPER_RSP {json.dumps(fields, ensure_ascii=False)}")

    # ================================================================
    #  发送操作
    # ================================================================

    def _send_page(self, page: str):
        if not self._worker.isRunning():
            return
        self._worker.send_page(page)
        self._append_log("CMD", f"PAGE -> {page}")

    def _send_current_state(self):
        if not self._worker.isRunning():
            return
        fields = {
            "current": self._sp_current.value(),
            "x": self._sp_x.value(),
            "y": self._sp_y.value(),
            "theta": self._sp_theta.value(),
            "bat": self._sp_bat.value(),
            "mode": self._edit_mode.text(),
            "action": self._edit_action.text(),
            "error": self._edit_error.text(),
        }
        self._worker.send_state(**fields)
        self._append_log("CMD", f"STATE {json.dumps(fields, ensure_ascii=False)}")

    def _send_quick_cmd(self, cmd: str):
        if not self._worker.isRunning():
            return
        self._worker.send_cmd({"cmd": cmd})
        self._append_log("CMD", cmd)

    def _send_custom_cmd(self):
        if not self._worker.isRunning():
            return
        text = self._cmd_edit.text().strip()
        if not text:
            return
        try:
            payload = json.loads(text)
            self._worker.send_cmd(payload)
            self._append_log("CMD", text)
        except json.JSONDecodeError as e:
            QMessageBox.warning(self, "JSON 错误", str(e))

    def _send_raw_json(self, json_str: str):
        if not self._worker.isRunning():
            return
        try:
            payload = json.loads(json_str)
            self._worker.send_cmd(payload)
            self._append_log("CMD", json_str)
        except json.JSONDecodeError:
            pass

    # ---- 心跳 ----
    def _toggle_heartbeat(self, checked: bool):
        if checked:
            self._heartbeat_timer.start(1000)
        else:
            self._heartbeat_timer.stop()

    def _send_heartbeat(self):
        if not self._worker.isRunning():
            return
        self._send_current_state()

    # ================================================================
    #  日志
    # ================================================================

    def _on_log(self, direction: str, hex_str: str):
        if self._log_check.isChecked():
            self._append_log(direction, hex_str)

    def _append_log(self, tag: str, msg: str):
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        colors = {
            "TX": "#2196F3",
            "RX": "#4CAF50",
            "CMD": "#FF9800",
            "SYS": "#9E9E9E",
            "ERR": "#F44336",
            "QUEUED": "#9C27B0",
        }
        color = colors.get(tag, "#888")
        self._log_text.append(
            f'<span style="color:{color}">[{ts}] [{tag}] {msg}</span>'
        )
        # 自动滚动到底部
        cursor = self._log_text.textCursor()
        cursor.movePosition(QTextCursor.End)
        self._log_text.setTextCursor(cursor)

    def _refresh_state_view(self):
        """刷新状态视图面板"""
        lines = []
        for k, v in sorted(self._state.items()):
            lines.append(f"  {k}: {v}")
        # 不覆盖，只追加变化
        self._state_text.moveCursor(QTextCursor.End)

    # ================================================================
    #  窗口关闭
    # ================================================================

    def closeEvent(self, event):
        self._heartbeat_timer.stop()
        if self._worker.isRunning():
            self._worker.disconnect_port()
            self._worker.wait(2000)
        event.accept()
