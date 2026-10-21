# VDA5050 协议映射指南

## 概述

本文档说明如何将 VDA5050 协议的 state 消息映射到 ESP32 HMI 显示系统。

## VDA5050 State 消息结构

VDA5050 定义的 state 消息包含以下核心字段：

```json
{
  "header": {
    "version": "2.0.0",
    "timestamp": "2026-10-13T10:30:00Z"
  },
  "serialNumber": "AGV-001",
  "operatingMode": "AUTOMATIC",
  "agvPosition": {
    "x": 12.345,
    "y": -4.567,
    "theta": 45.2
  },
  "velocity": {
    "vx": 1.2,
    "vy": 0.0,
    "omega": 0.0
  },
  "batteryState": {
    "batteryCharge": 0.86,
    "batteryVoltage": 48.5,
    "batteryState": "GOOD"
  },
  "orderState": {
    "orderId": "ORDER-12345",
    "orderState": "ACTIVE"
  },
  "actionStates": [
    {
      "actionId": "ACT-001",
      "actionType": "MOVING",
      "actionState": "RUNNING"
    }
  ],
  "errors": [],
  "safetyState": {
    "eStop": "OFF",
    "fieldViolation": false
  }
}
```

## 字段映射表

| VDA5050 字段 | ESP32 STATE 字段 | 转换说明 |
|-------------|-----------------|---------|
| `agvPosition.x` | `x` | 单位转换: m → mm (×1000) |
| `agvPosition.y` | `y` | 单位转换: m → mm (×1000) |
| `agvPosition.theta` | `theta` | 直接映射，单位 ° |
| `velocity.vx` | `current` | 根据轮径转换为 rpm，或映射为速度百分比 |
| `batteryState.batteryCharge` | `bat` | 单位转换: 0~1 → 0~100 (×100) |
| `batteryState.batteryVoltage` | `voltage` | 直接映射，单位 V |
| `operatingMode` | `mode` | 映射枚举值到短文本 |
| `orderState.orderId` | `rack` 或独立显示 | 截取前12字符 |
| `actionStates[0].actionType` | `action` | 取第一个动作的类型 |
| `errors[0].errorType` | `error` | 取第一个错误代码，无错误为空 |
| `safetyState.eStop` | `status` | 急停时显示 "ESTOP" |

## 操作模式映射

| VDA5050 operatingMode | ESP32 mode 显示 |
|----------------------|----------------|
| `AUTOMATIC` | `AUTO` |
| `SEMIAUTOMATIC` | `SEMI` |
| `MANUAL` | `MANU` |
| `SERVICE` | `SERV` |
| `ERROR` | `ERR` |

## 动作状态映射

| VDA5050 actionType | ESP32 action 显示 |
|-------------------|------------------|
| `MOVING` | `MOVING` |
| `CHARGING` | `CHARGE` |
| `LIFTING` | `LIFT` |
| `STOPPED` | `STOP` |
| `IDLE` | `IDLE` |
| `WAITING` | `WAIT` |

## Python 转换示例

```python
import json
import struct
import serial

HEADER = b"\xAA\x55"
TYPE_CMD = 0x01

def calc_checksum(frame_type: int, data: bytes) -> int:
    checksum = frame_type
    for byte in data:
        checksum ^= byte
    return checksum

def build_frame(frame_type: int, payload: dict) -> bytes:
    data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    return HEADER + bytes([frame_type]) + struct.pack("<H", len(data)) + data + bytes([calc_checksum(frame_type, data)])

def vda5050_to_esp32(vda_state: dict) -> dict:
    """将 VDA5050 state 消息转换为 ESP32 STATE 命令"""
    esp32_payload = {"cmd": "STATE"}
    
    # 位置转换 (m → mm)
    if "agvPosition" in vda_state:
        pos = vda_state["agvPosition"]
        esp32_payload["x"] = round(pos.get("x", 0) * 1000, 1)
        esp32_payload["y"] = round(pos.get("y", 0) * 1000, 1)
        esp32_payload["theta"] = round(pos.get("theta", 0), 1)
    
    # 速度转换 (m/s → rpm 显示)
    if "velocity" in vda_state:
        vel = vda_state["velocity"]
        vx = vel.get("vx", 0)
        # 示例: 假设轮径 0.2m, 1m/s ≈ 95.5rpm
        wheel_diameter = 0.2  # 米
        wheel_circumference = 3.14159 * wheel_diameter
        rpm = abs(vx) / wheel_circumference * 60
        esp32_payload["current"] = round(rpm, 1)
    
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
    
    # 订单状态
    if "orderState" in vda_state:
        order = vda_state["orderState"]
        if order.get("orderId"):
            esp32_payload["rack"] = str(order["orderId"])[:12]
    
    # 动作状态
    if "actionStates" in vda_state and vda_state["actionStates"]:
        action = vda_state["actionStates"][0]
        action_type = action.get("actionType", "IDLE")
        esp32_payload["action"] = action_type[:8]
    else:
        esp32_payload["action"] = "IDLE"
    
    # 错误状态
    if "errors" in vda_state and vda_state["errors"]:
        error = vda_state["errors"][0]
        esp32_payload["error"] = str(error.get("errorType", ""))[:12]
    else:
        esp32_payload["error"] = ""
    
    # 安全状态
    if "safetyState" in vda_state:
        safety = vda_state["safetyState"]
        if safety.get("eStop") == "ON" or safety.get("eStop") == "ACTIVE":
            esp32_payload["status"] = "ESTOP"
        elif safety.get("fieldViolation"):
            esp32_payload["status"] = "FIELD"
        else:
            # 根据操作模式设置状态
            esp32_payload["status"] = esp32_payload.get("mode", "AUTO")
    
    # 运行状态
    if "orderState" in vda_state:
        order = vda_state["orderState"]
        esp32_payload["run"] = order.get("orderState") == "ACTIVE"
    
    return esp32_payload

def send_state(port: serial.Serial, **fields):
    payload = {"cmd": "STATE"}
    payload.update(fields)
    port.write(build_frame(TYPE_CMD, payload))

# 使用示例
if __name__ == "__main__":
    # 模拟 VDA5050 state 消息
    vda5050_state = {
        "serialNumber": "AGV-001",
        "operatingMode": "AUTOMATIC",
        "agvPosition": {
            "x": 12.345,
            "y": -4.567,
            "theta": 45.2
        },
        "velocity": {
            "vx": 1.2,
            "vy": 0.0,
            "omega": 0.0
        },
        "batteryState": {
            "batteryCharge": 0.86,
            "batteryVoltage": 48.5
        },
        "orderState": {
            "orderId": "ORDER-12345",
            "orderState": "ACTIVE"
        },
        "actionStates": [
            {
                "actionType": "MOVING",
                "actionState": "RUNNING"
            }
        ],
        "errors": [],
        "safetyState": {
            "eStop": "OFF",
            "fieldViolation": False
        }
    }
    
    # 转换为 ESP32 格式
    esp32_data = vda5050_to_esp32(vda5050_state)
    print("ESP32 payload:", json.dumps(esp32_data, indent=2))
    
    # 发送到 ESP32
    with serial.Serial("COM7", 115200, timeout=0.05) as port:
        send_state(port, **esp32_data)
```

## 周期发送建议

VDA5050 建议 state 消息发送频率为 1-5 Hz。对于 ESP32 HMI 显示：

- **推荐频率**: 5-10 Hz (每 100-200ms 一次)
- **必须字段**: `current`, `x`, `y`, `bat`, `mode`, `action`
- **可选字段**: `theta`, `voltage`, `error` (变化时发送)

## 完整 STATE 命令示例

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
  "status": "AUTO",
  "mode": "AUTO",
  "action": "MOVING",
  "error": "",
  "pos": "X:12345 Y:-4567",
  "lift": "50",
  "rack": "ORDER-12345",
  "ip": "192.168.1.100"
}
```

## 注意事项

1. **单位转换**: VDA5050 使用米(m)，ESP32 显示使用毫米(mm)
2. **字段长度限制**: 字符串字段有长度限制，注意截断
3. **JSON 大小**: 单帧 JSON 控制在 256 字节以内
4. **变化检测**: ESP32 只在值真正变化时刷新显示，避免闪烁
5. **错误处理**: 无错误时 `error` 字段发送空字符串
