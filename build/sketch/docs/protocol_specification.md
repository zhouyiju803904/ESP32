#line 1 "C:\\Users\\user\\Desktop\\ESP32\\docs\\protocol_specification.md"
# ESP32 HMI 串口通讯协议 V2.0

## 1. 概述

本协议用于上位机与 ESP32-S3 HMI 显示终端通讯，支持 VDA5050 AGV 状态数据显示。

**基础参数：**
- 波特率：`115200`
- 数据格式：8N1
- 最大 JSON 数据体长度：`256` 字节

---

## 2. 帧格式

```
AA 55 TYPE LEN_LO LEN_HI JSON_DATA CHECKSUM
```

| 字段 | 长度 | 说明 |
|------|------|------|
| 帧头 | 2 字节 | 固定 `0xAA 0x55` |
| 帧类型 | 1 字节 | `0x01` 命令 / `0x02` 响应 / `0x03` 上报 |
| 数据长度 | 2 字节 | JSON 数据体长度，小端序 |
| 数据 | N 字节 | UTF-8 JSON 字符串，不含 `\0` |
| 校验 | 1 字节 | 从帧类型到数据末尾的 XOR 累加值 |

### 2.1 帧类型

| 值 | 标识 | 方向 | 用途 |
|----|------|------|------|
| `0x01` | CMD | 上位机 → ESP32 | 下发状态、切换页面、控制命令 |
| `0x02` | ACK | ESP32 → 上位机 | 命令确认响应 |
| `0x03` | RPT | ESP32 → 上位机 | 屏幕本地输入产生的参数/操作请求 |

### 2.2 校验算法

```
checksum = TYPE
for each byte in DATA:
    checksum = checksum XOR byte
```

注意：长度字节 `LEN_LO/LEN_HI` 不参与校验。

---

## 3. 上位机下发命令 (CMD)

### 3.1 STATE - 下发 AGV 状态（推荐）

用于周期下发 VDA5050 state 数据到 HMI 显示。所有字段均为可选，只发送需要更新的字段。

**命令格式：**
```json
{
  "cmd": "STATE",
  "current": 95.5,
  "spd": 100.0,
  "limit": 220.0,
  "accel": 2.5,
  "x": 12345.0,
  "y": -4567.0,
  "z": 0.0,
  "theta": 45.2,
  "bat": 86,
  "voltage": 48.5,
  "run": true,
  "mode": "AUTO",
  "action": "MOVING",
  "error": "",
  "pos": "X:12345 Y:-4567",
  "lift": "50",
  "rack": "ORDER-12345",
  "ip": "192.168.1.100"
}
```

**字段说明：**

| 字段 | 类型 | 显示位置 | 说明 |
|------|------|----------|------|
| `current` | float | STATUS 页 SPEED | 当前速度（无单位） |
| `spd` | float | PARAM 页 TARGET SPEED | 目标速度 |
| `limit` | float | PARAM 页 SPEED LIMIT | 速度上限 |
| `accel` | float | PARAM 页 ACCEL TIME | 加速时间（秒） |
| `x` | float | STATUS 页 POSITION X | X 坐标（mm） |
| `y` | float | STATUS 页 POSITION Y | Y 坐标（mm） |
| `z` | float | STATUS 页 POSITION Z | Z 坐标（mm） |
| `theta` | float | STATUS 页 POSITION T | 朝向角（°） |
| `bat` | int | Header 电池图标 | 电池电量（0-100%） |
| `voltage` | float | - | 电池电压（V），预留字段 |
| `run` | bool | OPER 页 STATUS | 运行状态（true=RUNNING/false=STOPPED） |
| `mode` | string | STATUS 页 MODE | 操作模式（AUTO/SEMI/MANU/SERV） |
| `action` | string | STATUS 页 ACTION | 动作状态（MOVING/IDLE/CHARGING等） |
| `error` | string | STATUS 页 ERROR | 错误代码（空字符串显示"---"） |
| `pos` | string | - | 位置文本，预留字段 |
| `lift` | string | OPER 页 LIFT HEIGHT | 举升高度文本 |
| `rack` | string | - | 货架/订单号文本，预留字段 |
| `ip` | string | Header IP 显示 | IP 地址文本 |

**推荐发送频率：** 5-10 Hz（每 100-200ms）

### 3.2 PAGE - 页面切换

```json
{"cmd": "PAGE", "val": "STATUS"}
```

`val` 可选值：
- `STATUS` / `STAT` - 状态页
- `PARAM` - 参数页
- `OPER` / `OP` - 操作页

### 3.3 兼容命令

以下命令仍可用，主要用于兼容旧上位机或简单调试。新状态显示推荐统一使用 `STATE`。

```json
{"cmd": "SPEED", "val": 120.0}
{"cmd": "LIMIT", "val": 220.0}
{"cmd": "ACCEL", "val": 2.5}
{"cmd": "X", "val": 12.30}
{"cmd": "Y", "val": -4.50}
{"cmd": "Z", "val": 8.20}
{"cmd": "START"}
{"cmd": "STOP"}
{"cmd": "HOME"}
{"cmd": "QUERY"}
```

---

## 4. ESP32 上报请求 (RPT)

ESP32 不主动周期上报状态；只有用户在屏幕上进行参数输入或操作请求时，才发送 RPT 帧。

### 4.1 参数页输入上报

参数页可点击以下字段输入数字：
- `TARGET SPEED`
- `SPEED LIMIT`
- `ACCEL TIME`

用户输入并点击 `OK` 后，ESP32 发送：

```json
{"rpt": "PARAM", "field": "spd", "val": 120.00}
{"rpt": "PARAM", "field": "limit", "val": 220.00}
{"rpt": "PARAM", "field": "accel", "val": 2.50}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `rpt` | string | 固定为 `PARAM` |
| `field` | string | `spd` / `limit` / `accel` |
| `val` | float | 用户输入并经 ESP32 限制后的值 |

上位机收到后负责真正应用参数，并可再通过 `STATE` 下发最新状态到 ESP32 显示。

### 4.2 操作页举升高度上报

操作页可点击 `LIFT HEIGHT` 区域输入高度。

用户输入高度后点击 `OK`，ESP32 发送：

```json
{"rpt": "OPER", "cmd": "LIFT_TO", "height": 50.00}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `rpt` | string | 固定为 `OPER` |
| `cmd` | string | 固定为 `LIFT_TO` |
| `height` | float | 用户输入的目标高度，单位 mm |

上位机收到后负责执行动作，并可通过 `STATE` 下发运行状态、举升高度等显示数据。

---

## 5. ACK 响应

ESP32 收到 CMD 后可返回 ACK 响应帧，帧类型为 `0x02`。

**成功示例：**
```json
{"cmd": "PAGE", "ack": "OK", "val": "STATUS"}
{"cmd": "STATE", "ack": "OK", "spd": 100.0, "current": 95.5, "bat": 86}
```

**错误示例：**
```json
{"cmd": "PARSE", "ack": "ERR", "msg": "json error"}
{"cmd": "PAGE", "ack": "ERR", "msg": "unknown page"}
{"cmd": "UNKNOWN", "ack": "ERR", "msg": "unknown cmd"}
```

---

## 6. VDA5050 协议映射

### 6.1 字段映射表

| VDA5050 字段 | ESP32 STATE 字段 | 转换说明 |
|-------------|-----------------|---------|
| `agvPosition.x` | `x` | 单位转换: m → mm (×1000) |
| `agvPosition.y` | `y` | 单位转换: m → mm (×1000) |
| `agvPosition.theta` | `theta` | 直接映射，单位 ° |
| `velocity.vx` | `current` | 根据轮径转换为 rpm 或速度值 |
| `batteryState.batteryCharge` | `bat` | 单位转换: 0~1 → 0~100 (×100) |
| `batteryState.batteryVoltage` | `voltage` | 直接映射，单位 V |
| `operatingMode` | `mode` | 映射枚举值到短文本 |
| `orderState.orderId` | `rack` | 截取前12字符 |
| `actionStates[0].actionType` | `action` | 取第一个动作的类型 |
| `errors[0].errorType` | `error` | 取第一个错误代码，无错误为空 |

### 6.2 操作模式映射

| VDA5050 operatingMode | ESP32 mode 显示 |
|----------------------|----------------|
| `AUTOMATIC` | `AUTO` |
| `SEMIAUTOMATIC` | `SEMI` |
| `MANUAL` | `MANU` |
| `SERVICE` | `SERV` |
| `ERROR` | `ERR` |

### 6.3 动作状态映射

| VDA5050 actionType | ESP32 action 显示 |
|-------------------|------------------|
| `MOVING` | `MOVING` |
| `CHARGING` | `CHARGE` |
| `LIFTING` | `LIFT` |
| `STOPPED` | `STOP` |
| `IDLE` | `IDLE` |
| `WAITING` | `WAIT` |

---

## 7. 显示页面布局

### 7.1 状态页 (STATUS)

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

### 7.2 参数页 (PARAM)

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

### 7.3 操作页 (OPER)

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

---

## 8. Python 上位机示例

```python
import json
import struct
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
    import time
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
        print("举升到高度:", payload["height"])

# VDA5050 转换示例
def vda5050_to_esp32(vda_state: dict) -> dict:
    esp32_payload = {"cmd": "STATE"}
    
    # 位置转换 (m → mm)
    if "agvPosition" in vda_state:
        pos = vda_state["agvPosition"]
        esp32_payload["x"] = round(pos.get("x", 0) * 1000, 1)
        esp32_payload["y"] = round(pos.get("y", 0) * 1000, 1)
        esp32_payload["theta"] = round(pos.get("theta", 0), 1)
    
    # 速度转换
    if "velocity" in vda_state:
        vel = vda_state["velocity"]
        vx = vel.get("vx", 0)
        esp32_payload["current"] = round(abs(vx) * 100, 1)  # 示例转换
    
    # 电池状态
    if "batteryState" in vda_state:
        bat = vda_state["batteryState"]
        esp32_payload["bat"] = int(bat.get("batteryCharge", 0) * 100)
        esp32_payload["voltage"] = round(bat.get("batteryVoltage", 0), 1)
    
    # 操作模式
    mode_map = {
        "AUTOMATIC": "AUTO",
        "SEMIAUTOMATIC": "SEMI",
        "MANUAL": "MANU",
        "SERVICE": "SERV",
        "ERROR": "ERR"
    }
    if "operatingMode" in vda_state:
        mode = vda_state["operatingMode"]
        esp32_payload["mode"] = mode_map.get(mode, mode[:4])
    
    # 动作状态
    if "actionStates" in vda_state and vda_state["actionStates"]:
        action = vda_state["actionStates"][0]
        esp32_payload["action"] = action.get("actionType", "IDLE")[:8]
    else:
        esp32_payload["action"] = "IDLE"
    
    # 错误状态
    if "errors" in vda_state and vda_state["errors"]:
        error = vda_state["errors"][0]
        esp32_payload["error"] = str(error.get("errorType", ""))[:12]
    else:
        esp32_payload["error"] = ""
    
    return esp32_payload

if __name__ == "__main__":
    with serial.Serial("COM7", 115200, timeout=0.05) as port:
        # 发送 VDA5050 状态
        send_state(
            port,
            current=95.5,
            spd=100.0,
            limit=220.0,
            accel=2.5,
            x=12345.0,
            y=-4567.0,
            theta=45.2,
            bat=86,
            voltage=48.5,
            run=True,
            mode="AUTO",
            action="MOVING",
            error="",
            lift="50",
            ip="192.168.1.100"
        )
        
        # 接收 ESP32 上报
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

## 9. 调试建议

1. **状态显示**：建议上位机按 5-10 Hz 下发 `STATE`
2. **参数编辑**：ESP32 参数页发送的是用户输入请求，上位机应校验并应用
3. **操作请求**：ESP32 操作页发送的是操作请求，上位机应负责安全判断和动作执行
4. **帧大小**：单帧 JSON 需控制在 256 字节以内
5. **ACK 处理**：如果上位机不关心 ACK，也建议定期读取串口，避免接收缓存积压
6. **变化检测**：ESP32 只在值真正变化时刷新显示，避免心跳包导致闪烁

---

## 10. 版本历史

- **V2.0** (2026-10-13)
  - 添加 VDA5050 协议支持（theta, voltage, mode, action, error 字段）
  - 简化显示界面，移除仪表盘和说明文字
  - 支持全屏数字键盘编辑参数
  
- **V1.0** (初始版本)
  - 基础状态显示
  - 参数编辑功能
  - 页面切换
