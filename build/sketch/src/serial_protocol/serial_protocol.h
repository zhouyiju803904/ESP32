#line 1 "C:\\Users\\user\\Desktop\\ESP32\\src\\serial_protocol\\serial_protocol.h"
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>

/*
 * 帧格式 (二进制帧 + JSON 数据体)
 *
 * [0xAA] [0x55] [TYPE] [LEN_LO] [LEN_HI] [DATA ...] [CHECKSUM]
 *
 * 帧头   : 0xAA 0x55  (2 字节)
 * 帧类型 : 1 字节
 * 数据长度: 2 字节, 小端序 (JSON 字节数)
 * 数据   : JSON 字符串
 * 校验   : 1 字节, 从帧类型到数据末尾的 XOR 累加
 */

#define SP_HEADER_0       0xAA
#define SP_HEADER_1       0x55
#define SP_MAX_PAYLOAD    256

/* 帧类型 */
#define SP_TYPE_CMD       0x01   /* 上位机 → ESP32 命令 */
#define SP_TYPE_ACK       0x02   /* ESP32 → 上位机 响应 */
#define SP_TYPE_RPT       0x03   /* ESP32 → 上位机 主动上报 */

/* 解析状态机 */
typedef enum
{
    SP_STATE_HEADER0,
    SP_STATE_HEADER1,
    SP_STATE_TYPE,
    SP_STATE_LEN_LO,
    SP_STATE_LEN_HI,
    SP_STATE_DATA,
    SP_STATE_CHECKSUM
} SP_State;

/* ---- 应用层共享变量 (定义在 ESP32.ino) ---- */

/* 页面编号 (与 ESP32.ino 中 ScreenPage 枚举对应) */
#define SP_PAGE_STATUS    0
#define SP_PAGE_PARAM     1
#define SP_PAGE_OPERATION 2

extern uint8_t currentPage;
extern bool screenDirty;
extern bool dynamicDirty;
extern bool machineRunning;

extern float targetSpeed;
extern float displayedSpeed;
extern float positionX;
extern float positionY;
extern float positionZ;
extern float speedLimit;
extern float accelTime;
extern uint8_t batteryPercent;

extern char statusText[];
extern char statusPositionText[];
extern char liftHeightText[];
extern char rackIdText[];
extern char ipAddressText[];
extern char modeText[];
extern char actionText[];
extern char errorText[];
extern char nameText[];
extern float batteryVoltage;
extern float theta;

extern void selectPage(uint8_t page);

/* ---- 协议接口 ---- */

/* 初始化, 注册输入 Serial 口 (Stream 兼容 HardwareSerial / HWCDC) */
void SP_Init(Stream &serial);

/* 主循环中调用, 驱动字节接收状态机 */
void SP_Process();

/* 构建 ACK 响应帧, 帧数据缓存到内部缓冲区
 * extraJson 为附加的 JSON 键值对, 例: "\"val\":120.0"
 */
void SP_SendAck(const char *cmd, const char *status, const char *extraJson = "");

/* 构建主动上报帧 */
void SP_SendReport(const char *jsonBody);

/* 检查是否有待发送的帧 */
bool SP_HasPendingFrame();

/* 获取待发送帧数据, length 可接收帧字节数 */
const uint8_t *SP_GetFrame(uint16_t *length = nullptr);

/* 清除待发送标志 (写出后调用) */
void SP_ClearPending();

#endif /* SERIAL_PROTOCOL_H */
