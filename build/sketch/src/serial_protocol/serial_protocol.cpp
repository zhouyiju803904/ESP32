#line 1 "C:\\Users\\user\\Desktop\\ESP32\\src\\serial_protocol\\serial_protocol.cpp"
#include "serial_protocol.h"

/* ---- 内部状态 ---- */
static Stream *sp_serial = nullptr;
static SP_State sp_state = SP_STATE_HEADER0;
static uint8_t sp_rxType = 0;
static uint16_t sp_rxLen = 0;
static uint16_t sp_rxIdx = 0;
static uint8_t sp_rxChecksum = 0;
static uint8_t sp_payload[SP_MAX_PAYLOAD];

/* 发送帧队列 (5 槽循环缓冲, 防止 ACK/RPT 互相覆盖) */
#define SP_TX_QUEUE_SIZE 5
static uint8_t  sp_txQueue[SP_TX_QUEUE_SIZE][6 + SP_MAX_PAYLOAD];
static uint16_t sp_txQueueLen[SP_TX_QUEUE_SIZE];
static uint8_t  sp_txHead  = 0;   /* 读取位置 (待发送) */
static uint8_t  sp_txTail  = 0;   /* 写入位置 (下一个空位) */
static uint8_t  sp_txCount = 0;   /* 队列中帧数量 */

/* ---- 校验: 从帧类型到数据末尾 XOR 累加 ---- */
static uint8_t sp_calcChecksum(uint8_t type, const uint8_t *data, uint16_t len)
{
    uint8_t xorVal = type;
    for (uint16_t i = 0; i < len; i++)
    {
        xorVal ^= data[i];
    }
    return xorVal;
}

static bool sp_copyStringField(JsonDocument &doc, const char *name, char *target, size_t targetSize)
{
    const char *value = doc[name];
    if (value == nullptr)
    {
        return false;
    }

    strlcpy(target, value, targetSize);
    screenDirty = true;
    dynamicDirty = true;
    return true;
}

/* ---- 构建并入队发送帧 (队列满时丢弃最旧帧) ---- */
static void sp_enqueueFrame(uint8_t type, const char *jsonBody)
{
    uint16_t len = (uint16_t)strlen(jsonBody);
    uint16_t idx = 0;

    /* 队列满时弹出最旧的帧 */
    if (sp_txCount >= SP_TX_QUEUE_SIZE)
    {
        sp_txHead = (sp_txHead + 1) % SP_TX_QUEUE_SIZE;
        sp_txCount--;
    }

    uint8_t *buf = sp_txQueue[sp_txTail];
    buf[idx++] = SP_HEADER_0;
    buf[idx++] = SP_HEADER_1;
    buf[idx++] = type;
    buf[idx++] = (uint8_t)(len & 0xFF);
    buf[idx++] = (uint8_t)(len >> 8);
    memcpy(&buf[idx], jsonBody, len);
    idx += len;
    buf[idx++] = sp_calcChecksum(type, (uint8_t *)jsonBody, len);

    sp_txQueueLen[sp_txTail] = idx;
    sp_txTail = (sp_txTail + 1) % SP_TX_QUEUE_SIZE;
    sp_txCount++;
}

/* ---- 命令分发 ---- */
static void sp_dispatchCommand(uint8_t *data, uint16_t len)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (char *)data, len);
    if (err)
    {
        SP_SendAck("PARSE", "ERR", "\"msg\":\"json error\"");
        return;
    }

    const char *cmd = doc["cmd"];
    if (cmd == nullptr)
    {
        SP_SendAck("UNKNOWN", "ERR", "\"msg\":\"no cmd field\"");
        return;
    }

    /* ---- 页面切换 ---- */
    if (strcmp(cmd, "PAGE") == 0)
    {
        const char *val = doc["val"];
        if (val == nullptr)
        {
            SP_SendAck("PAGE", "ERR", "\"msg\":\"missing val\"");
            return;
        }
        if (strcmp(val, "STATUS") == 0 || strcmp(val, "STAT") == 0)
        {
            selectPage(SP_PAGE_STATUS);
            SP_SendAck("PAGE", "OK", "\"val\":\"STATUS\"");
        }
        else if (strcmp(val, "PARAM") == 0)
        {
            selectPage(SP_PAGE_PARAM);
            SP_SendAck("PAGE", "OK", "\"val\":\"PARAM\"");
        }
        else if (strcmp(val, "OPER") == 0 || strcmp(val, "OP") == 0)
        {
            selectPage(SP_PAGE_OPERATION);
            SP_SendAck("PAGE", "OK", "\"val\":\"OPER\"");
        }
        else
        {
            SP_SendAck("PAGE", "ERR", "\"msg\":\"unknown page\"");
        }
        return;
    }

    /* ---- 启动 / 停止 / 回零 ---- */
    if (strcmp(cmd, "START") == 0 || strcmp(cmd, "RUN") == 0)
    {
        machineRunning = true;
        selectPage(SP_PAGE_OPERATION);
        screenDirty = true;
        dynamicDirty = true;
        SP_SendAck("START", "OK");
        return;
    }

    if (strcmp(cmd, "STOP") == 0)
    {
        machineRunning = false;
        targetSpeed = 0.0f;
        selectPage(SP_PAGE_OPERATION);
        screenDirty = true;
        dynamicDirty = true;
        SP_SendAck("STOP", "OK");
        return;
    }

    if (strcmp(cmd, "HOME") == 0)
    {
        positionX = 0.0f;
        positionY = 0.0f;
        positionZ = 0.0f;
        selectPage(SP_PAGE_OPERATION);
        screenDirty = true;
        dynamicDirty = true;
        SP_SendAck("HOME", "OK");
        return;
    }

    /* ---- 速度 ---- */
    if (strcmp(cmd, "SPEED") == 0)
    {
        float val = doc["val"] | 0.0f;
        targetSpeed = constrain(val, 0.0f, speedLimit);
        machineRunning = targetSpeed > 0.0f;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.1f", targetSpeed);
        SP_SendAck("SPEED", "OK", extra);
        return;
    }

    /* ---- 限速 ---- */
    if (strcmp(cmd, "LIMIT") == 0)
    {
        float val = doc["val"] | 0.0f;
        speedLimit = constrain(val, 1.0f, 240.0f);
        targetSpeed = constrain(targetSpeed, 0.0f, speedLimit);
        selectPage(SP_PAGE_PARAM);
        screenDirty = true;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.1f", speedLimit);
        SP_SendAck("LIMIT", "OK", extra);
        return;
    }

    /* ---- 加速时间 ---- */
    if (strcmp(cmd, "ACCEL") == 0)
    {
        float val = doc["val"] | 0.0f;
        accelTime = constrain(val, 0.1f, 30.0f);
        selectPage(SP_PAGE_PARAM);
        screenDirty = true;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.1f", accelTime);
        SP_SendAck("ACCEL", "OK", extra);
        return;
    }

    /* ---- 坐标 ---- */
    if (strcmp(cmd, "X") == 0)
    {
        positionX = doc["val"] | 0.0f;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.2f", positionX);
        SP_SendAck("X", "OK", extra);
        return;
    }

    if (strcmp(cmd, "Y") == 0)
    {
        positionY = doc["val"] | 0.0f;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.2f", positionY);
        SP_SendAck("Y", "OK", extra);
        return;
    }

    if (strcmp(cmd, "Z") == 0)
    {
        positionZ = doc["val"] | 0.0f;
        dynamicDirty = true;
        char extra[48];
        snprintf(extra, sizeof(extra), "\"val\":%.2f", positionZ);
        SP_SendAck("Z", "OK", extra);
        return;
    }

    /* ---- 批量设置 ---- */
    if (strcmp(cmd, "SET") == 0 || strcmp(cmd, "STATE") == 0 || strcmp(cmd, "DATA") == 0)
    {
        /* 保存旧值, 用于检测是否真正变化 */
        float oldTargetSpeed = targetSpeed;
        float oldDisplayedSpeed = displayedSpeed;
        float oldPosX = positionX;
        float oldPosY = positionY;
        float oldPosZ = positionZ;
        uint8_t oldBat = batteryPercent;
        bool oldRunning = machineRunning;
        float oldLimit = speedLimit;
        float oldAccel = accelTime;
        float oldVoltage = batteryVoltage;
        float oldTheta = theta;
        char oldStatus[16];
        strlcpy(oldStatus, statusText, sizeof(oldStatus));
        char oldPos[24];
        strlcpy(oldPos, statusPositionText, sizeof(oldPos));
        char oldLift[12];
        strlcpy(oldLift, liftHeightText, sizeof(oldLift));
        char oldRack[20];
        strlcpy(oldRack, rackIdText, sizeof(oldRack));
        char oldIp[24];
        strlcpy(oldIp, ipAddressText, sizeof(oldIp));
        char oldMode[12];
        strlcpy(oldMode, modeText, sizeof(oldMode));
        char oldAction[16];
        strlcpy(oldAction, actionText, sizeof(oldAction));
        char oldError[16];
        strlcpy(oldError, errorText, sizeof(oldError));
        char oldName[24];
        strlcpy(oldName, nameText, sizeof(oldName));

        char parts[16][64];
        int partCount = 0;

        if (!doc["spd"].isNull())
        {
            float val = doc["spd"] | 0.0f;
            targetSpeed = constrain(val, 0.0f, speedLimit);
            machineRunning = targetSpeed > 0.0f;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"spd\":%.1f", targetSpeed);
        }
        if (!doc["current"].isNull())
        {
            displayedSpeed = doc["current"] | displayedSpeed;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"current\":%.1f", displayedSpeed);
        }
        if (!doc["actual"].isNull())
        {
            displayedSpeed = doc["actual"] | displayedSpeed;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"actual\":%.1f", displayedSpeed);
        }
        if (!doc["limit"].isNull())
        {
            float val = doc["limit"] | 0.0f;
            speedLimit = constrain(val, 1.0f, 240.0f);
            targetSpeed = constrain(targetSpeed, 0.0f, speedLimit);
            snprintf(parts[partCount++], sizeof(parts[0]), "\"limit\":%.1f", speedLimit);
        }
        if (!doc["accel"].isNull())
        {
            float val = doc["accel"] | 0.0f;
            accelTime = constrain(val, 0.1f, 30.0f);
            snprintf(parts[partCount++], sizeof(parts[0]), "\"accel\":%.1f", accelTime);
        }
        if (!doc["x"].isNull())
        {
            positionX = doc["x"] | 0.0f;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"x\":%.2f", positionX);
        }
        if (!doc["y"].isNull())
        {
            positionY = doc["y"] | 0.0f;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"y\":%.2f", positionY);
        }
        if (!doc["z"].isNull())
        {
            positionZ = doc["z"] | 0.0f;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"z\":%.2f", positionZ);
        }
        if (!doc["bat"].isNull())
        {
            batteryPercent = constrain(doc["bat"] | batteryPercent, 0, 100);
            snprintf(parts[partCount++], sizeof(parts[0]), "\"bat\":%u", batteryPercent);
        }
        if (!doc["run"].isNull())
        {
            machineRunning = doc["run"] | machineRunning;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"run\":%s", machineRunning ? "true" : "false");
        }
        if (sp_copyStringField(doc, "status", statusText, 16))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"status\":\"%s\"", statusText);
        }
        if (sp_copyStringField(doc, "pos", statusPositionText, 24))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"pos\":\"%s\"", statusPositionText);
        }
        if (sp_copyStringField(doc, "lift", liftHeightText, 12))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"lift\":\"%s\"", liftHeightText);
        }
        if (sp_copyStringField(doc, "rack", rackIdText, 20))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"rack\":\"%s\"", rackIdText);
        }
        if (sp_copyStringField(doc, "ip", ipAddressText, 24))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"ip\":\"%s\"", ipAddressText);
        }
        /* VDA5050 扩展字段 */
        if (!doc["theta"].isNull())
        {
            theta = doc["theta"] | theta;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"theta\":%.1f", theta);
        }
        if (!doc["voltage"].isNull())
        {
            batteryVoltage = doc["voltage"] | batteryVoltage;
            snprintf(parts[partCount++], sizeof(parts[0]), "\"voltage\":%.1f", batteryVoltage);
        }
        if (sp_copyStringField(doc, "mode", modeText, 12))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"mode\":\"%s\"", modeText);
        }
        if (sp_copyStringField(doc, "action", actionText, 16))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"action\":\"%s\"", actionText);
        }
        if (sp_copyStringField(doc, "error", errorText, 16))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"error\":\"%s\"", errorText);
        }
        if (sp_copyStringField(doc, "name", nameText, 24))
        {
            snprintf(parts[partCount++], sizeof(parts[0]), "\"name\":\"%s\"", nameText);
        }

        char extra[256];
        int pos = 0;
        for (int i = 0; i < partCount; i++)
        {
            int written = snprintf(&extra[pos], sizeof(extra) - pos, "%s%s", i > 0 ? "," : "", parts[i]);
            if (written < 0 || written >= (int)(sizeof(extra) - pos))
            {
                pos = sizeof(extra) - 1;
                break;
            }
            pos += written;
        }
        extra[pos] = '\0';

        /* 只有值真正变化时才标记重绘 (防止心跳包导致屏幕闪烁) */
        bool valuesChanged =
            targetSpeed != oldTargetSpeed ||
            displayedSpeed != oldDisplayedSpeed ||
            positionX != oldPosX || positionY != oldPosY || positionZ != oldPosZ ||
            batteryPercent != oldBat || machineRunning != oldRunning ||
            speedLimit != oldLimit || accelTime != oldAccel ||
            batteryVoltage != oldVoltage || theta != oldTheta ||
            strcmp(statusText, oldStatus) != 0 ||
            strcmp(statusPositionText, oldPos) != 0 ||
            strcmp(liftHeightText, oldLift) != 0 ||
            strcmp(rackIdText, oldRack) != 0 ||
            strcmp(ipAddressText, oldIp) != 0 ||
            strcmp(modeText, oldMode) != 0 ||
            strcmp(actionText, oldAction) != 0 ||
            strcmp(errorText, oldError) != 0 ||
            strcmp(nameText, oldName) != 0;

        if (valuesChanged)
        {
            dynamicDirty = true;
        }

        SP_SendAck("SET", "OK", extra);
        return;
    }

    /* ---- 查询全部状态 ---- */
    if (strcmp(cmd, "QUERY") == 0)
    {
        char body[256];
        snprintf(body, sizeof(body),
                 "\"run\":%s,\"spd\":%.1f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
                 "\"bat\":%u,\"page\":%d,\"status\":\"%s\"",
                 machineRunning ? "true" : "false",
                 displayedSpeed, positionX, positionY, positionZ,
                 batteryPercent, (int)currentPage + 1, statusText);
        SP_SendAck("QUERY", "OK", body);
        return;
    }

    /* ---- 未知命令 ---- */
    SP_SendAck("UNKNOWN", "ERR", "\"msg\":\"unknown cmd\"");
}

/* ---- 公开接口 ---- */

void SP_Init(Stream &serial)
{
    sp_serial = &serial;
    sp_state = SP_STATE_HEADER0;
}

void SP_Process()
{
    if (sp_serial == nullptr)
    {
        return;
    }

    while (sp_serial->available() > 0)
    {
        uint8_t byte = (uint8_t)sp_serial->read();

        switch (sp_state)
        {
        case SP_STATE_HEADER0:
            if (byte == SP_HEADER_0)
            {
                sp_state = SP_STATE_HEADER1;
            }
            break;

        case SP_STATE_HEADER1:
            sp_state = (byte == SP_HEADER_1) ? SP_STATE_TYPE : SP_STATE_HEADER0;
            break;

        case SP_STATE_TYPE:
            if (byte == SP_TYPE_CMD || byte == SP_TYPE_ACK || byte == SP_TYPE_RPT)
            {
                sp_rxType = byte;
                sp_rxChecksum = byte;
                sp_state = SP_STATE_LEN_LO;
            }
            else
            {
                sp_state = SP_STATE_HEADER0;
            }
            break;

        case SP_STATE_LEN_LO:
            sp_rxLen = byte;
            sp_state = SP_STATE_LEN_HI;
            break;

        case SP_STATE_LEN_HI:
            sp_rxLen |= ((uint16_t)byte << 8);
            if (sp_rxLen > SP_MAX_PAYLOAD)
            {
                sp_state = SP_STATE_HEADER0;
            }
            else if (sp_rxLen == 0)
            {
                sp_state = SP_STATE_CHECKSUM;
            }
            else
            {
                sp_rxIdx = 0;
                sp_state = SP_STATE_DATA;
            }
            break;

        case SP_STATE_DATA:
            sp_payload[sp_rxIdx++] = byte;
            sp_rxChecksum ^= byte;
            if (sp_rxIdx >= sp_rxLen)
            {
                sp_state = SP_STATE_CHECKSUM;
            }
            break;

        case SP_STATE_CHECKSUM:
            if (byte == sp_rxChecksum)
            {
                if (sp_rxType == SP_TYPE_CMD)
                {
                    sp_dispatchCommand(sp_payload, sp_rxLen);
                }
            }
            sp_state = SP_STATE_HEADER0;
            break;
        }
    }
}

void SP_SendAck(const char *cmd, const char *status, const char *extraJson)
{
    char payload[SP_MAX_PAYLOAD];
    if (extraJson != nullptr && extraJson[0] != '\0')
    {
        snprintf(payload, sizeof(payload),
                 "{\"cmd\":\"%s\",\"ack\":\"%s\",%s}", cmd, status, extraJson);
    }
    else
    {
        snprintf(payload, sizeof(payload),
                 "{\"cmd\":\"%s\",\"ack\":\"%s\"}", cmd, status);
    }
    sp_enqueueFrame(SP_TYPE_ACK, payload);
}

void SP_SendReport(const char *jsonBody)
{
    sp_enqueueFrame(SP_TYPE_RPT, jsonBody);
}

bool SP_HasPendingFrame()
{
    return sp_txCount > 0;
}

const uint8_t *SP_GetFrame(uint16_t *length)
{
    if (length != nullptr)
    {
        *length = sp_txQueueLen[sp_txHead];
    }
    return sp_txQueue[sp_txHead];
}

void SP_ClearPending()
{
    if (sp_txCount > 0)
    {
        sp_txHead = (sp_txHead + 1) % SP_TX_QUEUE_SIZE;
        sp_txCount--;
    }
}
