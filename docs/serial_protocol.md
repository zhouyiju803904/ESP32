# ESP32 HMI 串口通讯协议

## 1. 概述

本协议用于上位机与 ESP32-S3 HMI 显示终端通讯。当前项目采用混合职责：

- 状态页显示数据由上位机下发，ESP32 负责显示。
- 参数页允许用户在 ESP32 屏幕输入参数，ESP32 将输入结果发送给上位机。
- 操作页允许用户在 ESP32 屏幕输入操作参数并触发操作命令，ESP32 将操作请求发送给上位机。

基础参数：

- 波特率：`115200`
- 数据格式：8N1
- 最大 JSON 数据体长度：`256` 字节

---

## 2. 帧格式

```text
AA 55 TYPE LEN_LO LEN_HI JSON_DATA CHECKSUM
```

| 字段 | 长度 | 说明 |
| --- | --- | --- |
| 帧头 | 2 字节 | 固定 `0xAA 0x55` |
| 帧类型 | 1 字节 | `0x01` 命令 / `0x02` 响应 / `0x03` 上报 |
| 数据长度 | 2 字节 | JSON 数据体长度，小端序 |
| 数据 | N 字节 | UTF-8 JSON 字符串，不含 `\0` |
| 校验 | 1 字节 | 从帧类型到数据末尾的 XOR 累加值 |

### 2.1 帧类型

| 值 | 标识 | 方向 | 用途 |
| --- | --- | --- | --- |
| `0x01` | CMD | 上位机 -> ESP32 | 下发状态、切换页面、兼容控制命令 |
| `0x02` | ACK | ESP32 -> 上位机 | 命令确认 |
| `0x03` | RPT | ESP32 -> 上位机 | 屏幕本地输入产生的参数/操作请求 |

### 2.2 校验算法

校验字节 = `TYPE XOR DATA[0] XOR DATA[1] ... XOR DATA[N-1]`。

```text
checksum = TYPE
for each byte in DATA:
    checksum = checksum XOR byte
```

注意：长度字节 `LEN_LO/LEN_HI` 不参与校验。

---

## 3. 上位机下发到 ESP32

所有命令帧的 JSON 必须包含 `"cmd"` 字段。

### 3.1 STATE / DATA / SET - 下发显示状态

`STATE`、`DATA`、`SET` 三个命令等价，推荐上位机使用 `STATE` 表达"下发当前状态"。所有字段均为可选，只发送需要更新的字段即可。

```json
{"cmd":"STATE","current":88.5,"spd":100.0,"limit":220.0,"accel":2.5,"x":12.3,"y":-4.5,"z":8.2,"bat":86,"run":true,"status":"RUN","lift":"50","rack":"A01","ip":"192.168.4.1","theta":45.0,"voltage":48.5,"mode":"AUTO","action":"MOVING","error":""}
```

| 字段 | 类型 | 显示含义 | 备注 |
| --- | --- | --- | --- |
| `current` | float | 当前速度，仪表盘实际显示值 | 推荐由上位机周期下发 |
| `actual` | float | 当前速度别名 | 与 `current` 等价 |
| `spd` | float | 目标速度 | 范围 `0.0 ~ limit` |
| `limit` | float | 速度上限 | 限制到 `1.0 ~ 240.0` |
| `accel` | float | 加速时间 | 限制到 `0.1 ~ 30.0` |
| `x` / `y` / `z` | float | 坐标显示 | 单位 mm |
| `bat` | int | 电池百分比 | 限制到 `0 ~ 100` |
| `run` | bool | 运行状态 | 用于运行/停止显示 |
| `status` | string | 状态文本 | 最长约 15 字符 |
| `pos` | string | 状态页位置文本 | 最长约 23 字符 |
| `lift` | string | 举升高度文本 | 最长约 11 字符 |
| `rack` | string | 货架/库位编号文本 | 最长约 19 字符 |
| `ip` | string | 顶部 IP 文本 | 最长约 23 字符 |
| `theta` | float | 车辆朝向角 | VDA5050 扩展，单位 ° |
| `voltage` | float | 电池电压 | VDA5050 扩展，单位 V |
| `mode` | string | 操作模式 | VDA5050 扩展，如 AUTO/SEMI/MANU/SERV |
| `action` | string | 当前动作状态 | VDA5050 扩展，如 MOVING/IDLE/CHARGING |
| `error` | string | 错误代码 | VDA5050 扩展，空字符串表示无错误 |

ESP32 收到后会更新内部显示变量。字符串字段和页面切换会触发静态区域重绘；数值字段会在下一次动态刷新中显示。

### 3.2 PAGE - 页面切换

```json
{"cmd":"PAGE","val":"STATUS"}
```

`val` 可选值：

| 值 | 页面 |
| --- | --- |
| `STATUS` / `STAT` | 状态页 |
| `PARAM` | 参数页 |
| `OPER` / `OP` | 操作页 |

### 3.3 兼容命令

以下命令仍可用，主要用于兼容旧上位机或简单调试。新状态显示推荐统一使用 `STATE`。

```json
{"cmd":"SPEED","val":120.0}
{"cmd":"LIMIT","val":220.0}
{"cmd":"ACCEL","val":2.5}
{"cmd":"X","val":12.30}
{"cmd":"Y","val":-4.50}
{"cmd":"Z","val":8.20}
{"cmd":"START"}
{"cmd":"STOP"}
{"cmd":"HOME"}
{"cmd":"QUERY"}
```

---

## 4. ESP32 上报到上位机

ESP32 不主动周期上报状态；只有用户在屏幕上进行参数输入或操作请求时，才发送 RPT 帧。

### 4.1 参数页输入上报

参数页可点击以下字段输入数字：

- `TARGET SPEED`
- `SPEED LIMIT`
- `ACCEL TIME`

用户输入并点击 `OK` 后，ESP32 发送：

```json
{"rpt":"PARAM","field":"spd","val":120.00}
{"rpt":"PARAM","field":"limit","val":220.00}
{"rpt":"PARAM","field":"accel","val":2.50}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `rpt` | string | 固定为 `PARAM` |
| `field` | string | `spd` / `limit` / `accel` |
| `val` | float | 用户输入并经 ESP32 限制后的值 |

上位机收到后负责真正应用参数，并可再通过 `STATE` 下发最新状态到 ESP32 显示。

### 4.2 操作页顶升到指定高度

操作页当前提供：

- 高度输入框：点击 `LIFT HEIGHT` 输入高度。
- `LIFT TO` 按钮：发送顶升到指定高度请求。

用户输入高度后点击 `LIFT TO`，ESP32 发送：

```json
{"rpt":"OPER","cmd":"LIFT_TO","height":50.00}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `rpt` | string | 固定为 `OPER` |
| `cmd` | string | 固定为 `LIFT_TO` |
| `height` | float | 用户输入的目标高度，单位 mm |

上位机收到后负责执行动作，并可通过 `STATE` 下发运行状态、举升高度等显示数据。

---

## 5. ACK 响应

ESP32 收到 CMD 后可返回 ACK 响应帧，帧类型为 `0x02`。

成功示例：

```json
{"cmd":"PAGE","ack":"OK","val":"STATUS"}
{"cmd":"SET","ack":"OK","spd":100.0,"current":88.5,"bat":86,"status":"RUN"}
```

错误示例：

```json
{"cmd":"PARSE","ack":"ERR","msg":"json error"}
{"cmd":"PAGE","ack":"ERR","msg":"unknown page"}
{"cmd":"UNKNOWN","ack":"ERR","msg":"unknown cmd"}
```

---

## 6. Python 上位机示例

```python
import json
import struct
import time
import serial

HEADER = b"\xAA\x55"
TYPE_CMD = 0x01
TYPE_ACK = 0x02
TYPE_RPT = 0x03


def calc_checksum(frame_type: int, data: bytes) -> int:
    checksum = frame_type
    for byte in data:
        checksum ^= byte
    return checksum


def build_frame(frame_type: int, payload: dict) -> bytes:
    data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    return HEADER + bytes([frame_type]) + struct.pack("<H", len(data)) + data + bytes([calc_checksum(frame_type, data)])


def read_frame(port: serial.Serial, timeout: float = 0.2):
    deadline = time.time() + timeout
    while time.time() < deadline:
        first = port.read(1)
        if not first:
            continue
        if first != b"\xAA":
            continue
        second = port.read(1)
        if second != b"\x55":
            continue

        header = port.read(3)
        if len(header) != 3:
            return None
        frame_type = header[0]
        length = struct.unpack("<H", header[1:3])[0]
        payload = port.read(length + 1)
        if len(payload) != length + 1:
            return None

        data = payload[:-1]
        checksum = payload[-1]
        if calc_checksum(frame_type, data) != checksum:
            return None
        return frame_type, json.loads(data.decode("utf-8"))
    return None


def send_state(port: serial.Serial, **fields):
    payload = {"cmd": "STATE"}
    payload.update(fields)
    port.write(build_frame(TYPE_CMD, payload))


def handle_report(payload: dict):
    if payload.get("rpt") == "PARAM":
        print("参数输入:", payload["field"], payload["val"])
    elif payload.get("rpt") == "OPER" and payload.get("cmd") == "LIFT_TO":
        print("顶升到指定高度:", payload["height"])


if __name__ == "__main__":
    with serial.Serial("COM7", 115200, timeout=0.05) as port:
        send_state(
            port,
            current=88.5,
            spd=100.0,
            limit=220.0,
            accel=2.5,
            x=12.3,
            y=-4.5,
            z=8.2,
            bat=86,
            run=True,
            status="RUN",
            lift="50",
            rack="A01",
            ip="192.168.4.1",
            theta=45.0,      # VDA5050: 朝向角
            voltage=48.5,    # VDA5050: 电池电压
            mode="AUTO",     # VDA5050: 操作模式
            action="MOVING", # VDA5050: 动作状态
            error="",        # VDA5050: 错误代码
        )

        while True:
            frame = read_frame(port, timeout=0.1)
            if frame is None:
                continue
            frame_type, payload = frame
            if frame_type == TYPE_RPT:
                handle_report(payload)
            elif frame_type == TYPE_ACK:
                print("ACK:", payload)
```

---

## 7. 调试建议

- 状态显示建议由上位机按 5-10 Hz 下发 `STATE`。
- ESP32 参数页发送的是用户输入请求，上位机应校验并应用。
- ESP32 操作页发送的是操作请求，上位机应负责安全判断和动作执行。
- 单帧 JSON 需控制在 256 字节以内。
- 如果上位机不关心 ACK，也建议定期读取串口，避免接收缓存积压。

---

## 8. 状态页显示布局

状态页采用简洁的数值显示布局，移除了仪表盘图形，主要显示 VDA5050 state 数据：

```
┌─────────────────────────────────────────────────────────┐
│  AGV STATUS                                             │
├─────────────────────────────────────────────────────────┤
│  SPEED                           │  MODE                │
│  95.5                            │  AUTO                │
├─────────────────────────────────────────────────────────┤
│  POSITION                                               │
│  X:12345.0  Y:-4567.0  T:45.2                          │
├─────────────────────────────────────────────────────────┤
│  ACTION             │  ERROR                            │
│  MOVING             │  ---                              │
└─────────────────────────────────────────────────────────┘
```

显示字段说明：
- **SPEED**: 当前速度数值 (来自 `current` 字段，无单位)
- **MODE**: 操作模式 (来自 `mode` 字段，如 AUTO/SEMI/MANU)
- **POSITION**: 位置坐标 (来自 `x`, `y`, `theta` 字段，T 表示朝向角)
- **ACTION**: 当前动作状态 (来自 `action` 字段)
- **ERROR**: 错误代码 (来自 `error` 字段，无错误显示 "---")

注：电池电量在顶部 header 区域显示，状态页不再重复显示。

---

## 9. 参数页显示布局

参数页采用简洁的全屏卡片布局，移除了右侧说明区域：

```
┌─────────────────────────────────────────────────────────┐
│  PARAMETER SETTING                                      │
├─────────────────────────────────────────────────────────┤
│  TARGET SPEED               │  CURRENT SPEED            │
│  100.0                      │  95.5                     │
├─────────────────────────────────────────────────────────┤
│  SPEED LIMIT                │  ACCEL TIME               │
│  220                        │  2.5                      │
└─────────────────────────────────────────────────────────┘
```

显示字段说明：
- **TARGET SPEED**: 目标速度 (来自 `spd` 字段)
- **CURRENT SPEED**: 当前实际速度 (来自 `current` 字段)
- **SPEED LIMIT**: 速度上限 (来自 `limit` 字段)
- **ACCEL TIME**: 加速时间 (来自 `accel` 字段)

操作方式：点击对应区域可输入数值，输入后发送 PARAM 上报到上位机。

---

## 10. 操作页显示布局

操作页采用简洁的全屏卡片布局，移除了右侧说明区域和按钮：

```
┌─────────────────────────────────────────────────────────┐
│  BASIC OPERATION                                        │
├─────────────────────────────────────────────────────────┤
│  STATUS                     │  SPEED                    │
│  RUNNING                    │  95.5                     │
├─────────────────────────────────────────────────────────┤
│  LIFT HEIGHT                                          │
│  50.0                                                 │
└─────────────────────────────────────────────────────────┘
```

显示字段说明：
- **STATUS**: 运行状态 (RUNNING/STOPPED，来自 `run` 字段)
- **SPEED**: 当前速度 (来自 `current` 字段)
- **LIFT HEIGHT**: 举升目标高度 (来自 `lift` 字段)

操作方式：点击 LIFT HEIGHT 区域可输入高度值，输入后发送 OPER 上报到上位机。
