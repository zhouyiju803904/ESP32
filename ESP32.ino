#include "src/rgb_lcd_port/rgb_lcd_port.h"
#include "src/rgb_lcd_port/gui_paint/gui_paint.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/serial_protocol/serial_protocol.h"

uint16_t *frameBuffer = nullptr;
uint16_t *baseBuffer = nullptr;
uint16_t *regionBuffer = nullptr;

const uint16_t COLOR_BACKGROUND = 0x1082;
const uint16_t COLOR_PANEL = 0x2104;
const uint16_t COLOR_PANEL_DARK = 0x18C3;
const uint16_t COLOR_GRID = 0x39E7;
const uint16_t COLOR_TEXT = 0xE71C;
const uint16_t COLOR_MUTED = 0x8C71;
const uint16_t COLOR_ACCENT = 0x05FF;
const uint16_t COLOR_WARNING = 0xFDC0;
const uint16_t COLOR_DANGER = 0xF9A6;
const uint16_t COLOR_OK = 0x5FE0;

const float SPEED_MAX = 240.0f;
const int GAUGE_CENTER_X = 250;
const int GAUGE_CENTER_Y = 328;
const int GAUGE_RADIUS = 198;
const uint16_t GAUGE_REGION_X = 26;
const uint16_t GAUGE_REGION_Y = 72;
const uint16_t GAUGE_REGION_W = 474;
const uint16_t GAUGE_REGION_H = 380;
const uint16_t POSITION_REGION_X = 526;
const uint16_t POSITION_REGION_Y = 72;
const uint16_t POSITION_REGION_W = 252;
const uint16_t POSITION_REGION_H = 380;
const uint16_t STATUS_REGION_X = 594;
const uint16_t STATUS_REGION_Y = 8;
const uint16_t STATUS_REGION_W = 184;
const uint16_t STATUS_REGION_H = 42;
const uint16_t TAB_Y = 14;
const uint16_t TAB_H = 76;
const uint16_t TAB_W = 230;
const uint16_t TAB_GAP = 22;
const uint16_t TAB_STATUS_X = 36;
const uint16_t TAB_PARAM_X = TAB_STATUS_X + TAB_W + TAB_GAP;
const uint16_t TAB_OPER_X = TAB_PARAM_X + TAB_W + TAB_GAP;
const uint8_t GT911_ADDR_PRIMARY = 0x5D;
const uint8_t GT911_ADDR_SECONDARY = 0x14;
const char *FW_VERSION = "v0.1.2";
const uint32_t FRAME_PIXELS = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
const uint32_t FRAME_BYTES = FRAME_PIXELS * sizeof(uint16_t);
const uint32_t REGION_BUFFER_PIXELS = GAUGE_REGION_W * GAUGE_REGION_H;
const uint32_t FRAME_INTERVAL_MS = 60;
const uint32_t TOUCH_RELEASE_DEBOUNCE_MS = 120;
const bool REPORT_TOUCH_EVENTS = false;

enum ScreenPage
{
    PAGE_STATUS,
    PAGE_PARAM,
    PAGE_OPERATION
};

const uint8_t PAGE_COUNT = 3;

/* 返回按钮坐标 */
const uint16_t BACK_BTN_X0 = 28;
const uint16_t BACK_BTN_Y0 = 74;
const uint16_t BACK_BTN_X1 = 68;
const uint16_t BACK_BTN_Y1 = 108;



enum ParamEditField
{
    EDIT_NONE,
    EDIT_TARGET_SPEED,
    EDIT_SPEED_LIMIT,
    EDIT_ACCEL_TIME,
    EDIT_LIFT_HEIGHT,
    EDIT_NAME
};

uint32_t lastFrameMs = 0;
uint32_t lastLogMs = 0;
uint8_t currentPage = PAGE_STATUS;
bool screenDirty = true;
bool dynamicDirty = true;
bool gaugeRegionDirty = false;
bool statusRegionDirty = false;
float lastDisplayedSpeedRounded = -999.0f;
bool machineRunning = false;
bool touchWasPressed = false;
bool touchTracking = false;
bool touchSwipeTriggered = false;
bool touchTapHandled = false;
bool gestureTracking = false;
bool gestureSwipeTriggered = false;
bool touchReady = false;
uint8_t touchAddress = GT911_ADDR_PRIMARY;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint16_t touchLastX = 0;
uint32_t lastTouchSeenMs = 0;
uint32_t lastTouchLogMs = 0;
uint32_t lastGestureMs = 0;
uint16_t gestureStartX = 0;
uint16_t gestureStartY = 0;
uint16_t touchLastY = 0;
float targetSpeed = 1.0f;
float displayedSpeed = 1.0f;
float positionX = 126.40f;
float positionY = -42.75f;
float positionZ = 8.20f;
float speedLimit = SPEED_MAX;
float accelTime = 2.5f;
float liftTargetHeight = 50.0f;
ParamEditField editingField = EDIT_NONE;
char editBuffer[16] = {0};
uint8_t editBufferLen = 0;
bool paramEditorDirty = false;
char statusText[16] = "IDLE";
char statusPositionText[24] = "1234567890";
char liftHeightText[12] = "50";
char rackIdText[20] = "123456789";
char ipAddressText[24] = "192.168.4.1";
char modeText[12] = "AUTO";
char actionText[16] = "IDLE";
char errorText[16] = "";
char nameText[24] = "AGV-001";
uint8_t nameNavType = 0;  /* 0 = SLAM, 1 = QRCODE */
uint8_t batteryPercent = 86;
float batteryVoltage = 48.0f;
float theta = 0.0f;

uint16_t color565(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

uint16_t batteryColorForPercent(uint8_t percent)
{
    percent = constrain(percent, 0, 100);
    uint8_t red = static_cast<uint8_t>(255 - (percent * 95UL) / 100UL);
    uint8_t green = static_cast<uint8_t>((percent * 224UL) / 100UL);
    return color565(red, green, 0);
}

void fillRoundedPanel(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    Paint_DrawRectangle(x0, y0, x1, y1, color, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(x0, y0, x1, y1, COLOR_GRID, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
}

float angleForSpeed(float speed)
{
    speed = constrain(speed, 0.0f, SPEED_MAX);
    return 210.0f + (speed / SPEED_MAX) * 120.0f;
}

void pointOnGauge(float degrees, int radius, int *x, int *y)
{
    float radians = degrees * PI / 180.0f;
    *x = GAUGE_CENTER_X + static_cast<int>(cosf(radians) * radius);
    *y = GAUGE_CENTER_Y + static_cast<int>(sinf(radians) * radius);
}

void drawThickLine(int x0, int y0, int x1, int y1, uint16_t color, DOT_PIXEL width)
{
    Paint_DrawLine(x0, y0, x1, y1, color, width, LINE_STYLE_SOLID);
}

void drawGaugeArc()
{
    for (int speed = 0; speed <= SPEED_MAX; speed += 4)
    {
        int x0;
        int y0;
        int x1;
        int y1;
        float angle = angleForSpeed(static_cast<float>(speed));
        pointOnGauge(angle, GAUGE_RADIUS - 8, &x0, &y0);
        pointOnGauge(angle, GAUGE_RADIUS, &x1, &y1);

        uint16_t color = COLOR_OK;
        if (speed > 170)
        {
            color = COLOR_WARNING;
        }
        if (speed > 210)
        {
            color = COLOR_DANGER;
        }

        drawThickLine(x0, y0, x1, y1, color, DOT_PIXEL_5X5);
    }
}

void drawGaugeScale()
{
    char label[8];

    for (int speed = 0; speed <= SPEED_MAX; speed += 20)
    {
        int outerX;
        int outerY;
        int innerX;
        int innerY;
        int labelX;
        int labelY;
        float angle = angleForSpeed(static_cast<float>(speed));
        int tickLength = (speed % 40 == 0) ? 24 : 13;

        pointOnGauge(angle, GAUGE_RADIUS - 20, &outerX, &outerY);
        pointOnGauge(angle, GAUGE_RADIUS - 20 - tickLength, &innerX, &innerY);
        drawThickLine(innerX, innerY, outerX, outerY, COLOR_TEXT, (speed % 40 == 0) ? DOT_PIXEL_3X3 : DOT_PIXEL_2X2);

        if (speed % 40 == 0)
        {
            snprintf(label, sizeof(label), "%d", speed);
            pointOnGauge(angle, GAUGE_RADIUS - 64, &labelX, &labelY);
            Paint_DrawString_EN(labelX - 18, labelY - 10, label, &Font16, COLOR_MUTED, COLOR_BACKGROUND);
        }
    }
}

void drawGaugeNeedle(float speed)
{
    int needleX;
    int needleY;
    pointOnGauge(angleForSpeed(speed), GAUGE_RADIUS - 76, &needleX, &needleY);
    drawThickLine(GAUGE_CENTER_X, GAUGE_CENTER_Y, needleX, needleY, COLOR_ACCENT, DOT_PIXEL_5X5);
    Paint_DrawCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, 18, COLOR_ACCENT, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, 8, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void drawGaugeStatic()
{
    fillRoundedPanel(26, 72, 500, 452, COLOR_PANEL_DARK);
    Paint_DrawString_EN(52, 92, "SPEED", &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    Paint_DrawString_EN(52, 122, "rpm", &Font16, COLOR_MUTED, COLOR_PANEL_DARK);
    Paint_DrawCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_RADIUS, COLOR_GRID, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawCircle(GAUGE_CENTER_X, GAUGE_CENTER_Y, GAUGE_RADIUS - 34, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    drawGaugeArc();
    drawGaugeScale();
}

void drawGaugeDynamic(float speed)
{
    char value[16];

    snprintf(value, sizeof(value), "%03d", static_cast<int>(speed + 0.5f));
    Paint_DrawString_EN(214, 350, value, &Font48, COLOR_TEXT, COLOR_PANEL_DARK);
    Paint_DrawString_EN(280, 380, "rpm", &Font20, COLOR_MUTED, COLOR_PANEL_DARK);

    drawGaugeNeedle(speed);
}

void drawPositionRowStatic(uint16_t y, const char *axis, uint16_t accent)
{
    Paint_DrawRectangle(548, y, 760, y + 74, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(548, y, 760, y + 74, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(548, y, 598, y + 74, accent, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(562, y + 20, axis, &Font24, COLOR_BACKGROUND, accent);
    Paint_DrawString_EN(704, y + 45, "mm", &Font16, COLOR_MUTED, COLOR_PANEL_DARK);
}

void drawPositionValue(uint16_t y, float value)
{
    char valueText[20];

    snprintf(valueText, sizeof(valueText), "%7.2f", value);
    Paint_DrawString_EN(620, y + 14, valueText, &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
}

void drawPositionStatic()
{
    fillRoundedPanel(526, 72, 778, 452, COLOR_PANEL);
    Paint_DrawString_EN(552, 94, "POSITION", &Font24, COLOR_TEXT, COLOR_PANEL);
    Paint_DrawString_EN(552, 124, "ABS COORD", &Font16, COLOR_MUTED, COLOR_PANEL);

    drawPositionRowStatic(166, "X", COLOR_ACCENT);
    drawPositionRowStatic(252, "Y", COLOR_OK);
    drawPositionRowStatic(338, "Z", COLOR_WARNING);
}

void drawPositionDynamic(float x, float y, float z)
{
    drawPositionValue(166, x);
    drawPositionValue(252, y);
    drawPositionValue(338, z);
}

void restoreRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void uploadRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

void drawHeaderStatic()
{
    Paint_DrawRectangle(24, 8, 776, 50, COLOR_BACKGROUND, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawLine(24, 54, 776, 54, COLOR_GRID, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawString_EN(34, 12, ipAddressText, &Font24, COLOR_ACCENT, COLOR_BACKGROUND);
    Paint_DrawString_EN(356, 16, FW_VERSION, &Font16, COLOR_MUTED, COLOR_BACKGROUND);
    Paint_DrawString_EN(596, 20, "BAT", &Font16, COLOR_MUTED, COLOR_BACKGROUND);
    Paint_DrawRectangle(684, 14, 758, 40, COLOR_GRID, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(760, 21, 766, 33, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void drawBackButton()
{
    if (PAGE_COUNT < 2)
    {
        return;
    }
    Paint_DrawRectangle(BACK_BTN_X0, BACK_BTN_Y0, BACK_BTN_X1, BACK_BTN_Y1,
                        COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(BACK_BTN_X0, BACK_BTN_Y0, BACK_BTN_X1, BACK_BTN_Y1,
                        COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(BACK_BTN_X0 + 10, BACK_BTN_Y0 + 5, "<", &Font24,
                        COLOR_TEXT, COLOR_PANEL_DARK);
}


void drawHeaderDynamic(float speed)
{
    (void)speed;
    char batteryText[8];
    uint8_t percent = constrain(batteryPercent, 0, 100);
    uint16_t fillColor = batteryColorForPercent(percent);

    restoreRegion(STATUS_REGION_X, STATUS_REGION_Y, STATUS_REGION_W, STATUS_REGION_H);
    snprintf(batteryText, sizeof(batteryText), "%u%%", percent);
    Paint_DrawString_EN(636, 16, batteryText, &Font20, fillColor, COLOR_BACKGROUND);
    Paint_DrawRectangle(690, 20, 752, 34, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    if (percent > 0)
    {
        uint16_t fillWidth = static_cast<uint16_t>((58UL * percent) / 100UL);
        Paint_DrawRectangle(692, 22, 692 + fillWidth, 32, fillColor, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    }
    statusRegionDirty = true;
}

void drawFooter()
{
    Paint_DrawRectangle(24, 458, 776, 476, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(34, 462, "AUTO MODE", &Font12, COLOR_BACKGROUND, COLOR_GRID);
    Paint_DrawString_EN(156, 462, "SERVO READY", &Font12, COLOR_BACKGROUND, COLOR_GRID);
    Paint_DrawString_EN(308, 462, "LIMIT OK", &Font12, COLOR_BACKGROUND, COLOR_GRID);
    Paint_DrawString_EN(430, 462, "ENCODER ONLINE", &Font12, COLOR_BACKGROUND, COLOR_GRID);
}

void drawPageTabs(const char *activeName)
{
    (void)activeName;
}

void drawValueCard(uint16_t x, uint16_t y, uint16_t width, const char *title, const char *value, const char *unit, uint16_t accent)
{
    Paint_DrawRectangle(x, y, x + width, y + 84, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(x, y, x + width, y + 84, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(x, y, x + 8, y + 84, accent, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(x + 20, y + 12, title, &Font16, COLOR_MUTED, COLOR_PANEL_DARK);
    Paint_DrawString_EN(x + 20, y + 40, value, &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    Paint_DrawString_EN(x + width - 54, y + 52, unit, &Font12, COLOR_MUTED, COLOR_PANEL_DARK);
}

void drawStatusMetricStatic(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *title, uint16_t accent)
{
    Paint_DrawRectangle(x, y, x + width, y + height, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(x, y, x + width, y + height, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(x, y, x + 8, y + height, accent, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawString_EN(x + 22, y + 12, title, &Font16, COLOR_MUTED, COLOR_PANEL_DARK);
}

void drawStatusMetricValue(uint16_t x, uint16_t y, const char *value, const char *unit, sFONT *font)
{
    Paint_DrawString_EN(x + 22, y + 38, value, font, COLOR_TEXT, COLOR_PANEL_DARK);
    if (unit != nullptr && unit[0] != '\0')
    {
        Paint_DrawString_EN(x + 150, y + 54, unit, &Font16, COLOR_MUTED, COLOR_PANEL_DARK);
    }
}

void drawCommandHint(const char *line1, const char *line2, const char *line3)
{
    Paint_DrawRectangle(526, 102, 778, 452, COLOR_PANEL, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(526, 102, 778, 452, COLOR_GRID, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(552, 126, "HOST DATA", &Font20, COLOR_TEXT, COLOR_PANEL);
    Paint_DrawString_EN(552, 180, line1, &Font16, COLOR_ACCENT, COLOR_PANEL);
    Paint_DrawString_EN(552, 220, line2, &Font16, COLOR_OK, COLOR_PANEL);
    Paint_DrawString_EN(552, 260, line3, &Font16, COLOR_WARNING, COLOR_PANEL);
    Paint_DrawString_EN(552, 360, "Display follows serial input", &Font12, COLOR_MUTED, COLOR_PANEL);
}

void drawStatusInfoStatic()
{
    fillRoundedPanel(26, 72, 778, 452, COLOR_PANEL_DARK);
    Paint_DrawString_EN(52, 94, "AGV STATUS", &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    
    /* 速度与模式 */
    drawStatusMetricStatic(52, 140, 500, 100, "SPEED", COLOR_ACCENT);
    drawStatusMetricStatic(580, 140, 180, 100, "MODE", COLOR_OK);
    
    /* 位置信息 */
    drawStatusMetricStatic(52, 260, 708, 80, "POSITION", COLOR_ACCENT);
    
    /* 动作与错误 */
    drawStatusMetricStatic(52, 360, 340, 72, "ACTION", COLOR_WARNING);
    drawStatusMetricStatic(420, 360, 340, 72, "ERROR", COLOR_DANGER);
}

void drawStatusInfoDynamic(float speed)
{
    char speedText[16];
    char posText[64];
    
    /* 速度 */
    snprintf(speedText, sizeof(speedText), "%.1f", speed);
    drawStatusMetricValue(52, 140, speedText, "", &Font48);
    
    /* 模式 */
    drawStatusMetricValue(580, 140, modeText, "", &Font24);
    
    /* 位置 */
    snprintf(posText, sizeof(posText), "X:%.1f  Y:%.1f  T:%.1f", positionX, positionY, theta);
    drawStatusMetricValue(52, 260, posText, "", &Font20);
    
    /* 动作 */
    drawStatusMetricValue(52, 360, actionText, "", &Font24);
    
    /* 错误 */
    drawStatusMetricValue(420, 360, errorText[0] ? errorText : "---", "", &Font24);
}

void drawStatusStatic()
{
    Paint_Clear(COLOR_BACKGROUND);
    drawHeaderStatic();
    drawStatusInfoStatic();
    drawFooter();
}

void drawStatusDynamic(float speed)
{
    drawHeaderDynamic(speed);
    drawStatusInfoDynamic(speed);
    statusRegionDirty = true;
}

bool isEditingParam()
{
    return editingField != EDIT_NONE;
}

void cancelParamEdit()
{
    if (editingField != EDIT_NONE)
    {
        editingField = EDIT_NONE;
        paramEditorDirty = false;
        screenDirty = true;
        dynamicDirty = true;
    }
}

const char *editFieldTitle()
{
    if (editingField == EDIT_TARGET_SPEED)
    {
        return "TARGET SPEED";
    }
    if (editingField == EDIT_SPEED_LIMIT)
    {
        return "SPEED LIMIT";
    }
    if (editingField == EDIT_ACCEL_TIME)
    {
        return "ACCEL TIME";
    }
    if (editingField == EDIT_LIFT_HEIGHT)
    {
        return "LIFT HEIGHT";
    }
    if (editingField == EDIT_NAME)
    {
        return "NAME";
    }
    return "PARAM";
}

const char *editFieldReportName()
{
    if (editingField == EDIT_TARGET_SPEED)
    {
        return "spd";
    }
    if (editingField == EDIT_SPEED_LIMIT)
    {
        return "limit";
    }
    if (editingField == EDIT_ACCEL_TIME)
    {
        return "accel";
    }
    if (editingField == EDIT_LIFT_HEIGHT)
    {
        return "height";
    }
    if (editingField == EDIT_NAME)
    {
        return "name";
    }
    return "unknown";
}

void drawNumericKey(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *label, uint16_t color)
{
    Paint_DrawRectangle(x, y, x + w, y + h, color, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(x, y, x + w, y + h, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(x + 20, y + 12, label, &Font20, COLOR_BACKGROUND, color);
}

void drawNumericEditor()
{
    bool nameMode = (editingField == EDIT_NAME);
    const char *keys[4][3] = {
        {"7", "8", "9"},
        {"4", "5", "6"},
        {"1", "2", "3"},
        {nameMode ? (nameNavType == 0 ? "SLAM" : "QRCODE") : "-",
         "0", nameMode ? "" : "."}};

    /* 全屏覆盖显示 */
    Paint_DrawRectangle(26, 72, 778, 452, COLOR_PANEL, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(26, 72, 778, 452, COLOR_GRID, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(52, 92, editFieldTitle(), &Font24, COLOR_TEXT, COLOR_PANEL);
    
    /* 输入框 */
    Paint_DrawRectangle(52, 130, 752, 180, COLOR_PANEL_DARK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
    Paint_DrawRectangle(52, 130, 752, 180, COLOR_GRID, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    if (nameMode)
    {
        char fullName[32];
        snprintf(fullName, sizeof(fullName), "%s-%s",
                 (nameNavType == 0) ? "SLAM" : "QRCODE",
                 editBufferLen > 0 ? editBuffer : "0");
        Paint_DrawString_EN(72, 142, fullName, &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    }
    else
    {
        Paint_DrawString_EN(72, 142, editBufferLen > 0 ? editBuffer : "0", &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    }

    /* 数字键盘 - 居中显示 */
    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t col = 0; col < 3; col++)
        {
            if (keys[row][col][0] == '\0') continue;
            drawNumericKey(100 + col * 100, 200 + row * 50, 80, 40, keys[row][col], COLOR_PANEL_DARK);
        }
    }

    /* 功能键 */
    drawNumericKey(100, 406, 80, 42, "DEL", COLOR_WARNING);
    drawNumericKey(200, 406, 80, 42, "OK", COLOR_OK);
    drawNumericKey(300, 406, 80, 42, "ESC", COLOR_DANGER);
}

void restoreRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    for (uint16_t row = 0; row < height; ++row)
    {
        uint32_t offset = (y + row) * EXAMPLE_LCD_H_RES + x;
        memcpy(frameBuffer + offset, baseBuffer + offset, width * sizeof(uint16_t));
    }
}

void uploadRegion(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    for (uint16_t row = 0; row < height; ++row)
    {
        uint32_t sourceOffset = (y + row) * EXAMPLE_LCD_H_RES + x;
        uint32_t targetOffset = row * width;
        memcpy(regionBuffer + targetOffset, frameBuffer + sourceOffset, width * sizeof(uint16_t));
    }

    waveshare_rgb_lcd_display_bitmap(x, y, x + width, y + height, reinterpret_cast<uint8_t *>(regionBuffer));
}

void drawParamStatic()
{
    Paint_Clear(COLOR_BACKGROUND);
    drawHeaderStatic();
    fillRoundedPanel(26, 72, 778, 452, COLOR_PANEL_DARK);
    Paint_DrawString_EN(52, 94, "PARAMETER SETTING", &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    
    /* 参数卡片 */
    drawStatusMetricStatic(52, 140, 340, 100, "TARGET SPEED", COLOR_ACCENT);
    drawStatusMetricStatic(420, 140, 340, 100, "CURRENT SPEED", COLOR_OK);
    drawStatusMetricStatic(52, 260, 340, 100, "SPEED LIMIT", COLOR_WARNING);
    drawStatusMetricStatic(420, 260, 340, 100, "ACCEL TIME", COLOR_DANGER);
    
    drawFooter();
}

void drawParamDynamic(float speed)
{
    /* 如果正在编辑，绘制数字键盘（覆盖在静态背景上） */
    if (isEditingParam())
    {
        if (paramEditorDirty)
        {
            drawNumericEditor();
            paramEditorDirty = false;
        }
        return;
    }

    /* 正常显示参数 */
    char targetText[16];
    char currentText[16];
    char limitText[16];
    char accelText[16];

    drawHeaderDynamic(speed);
    snprintf(targetText, sizeof(targetText), "%.1f", targetSpeed);
    snprintf(currentText, sizeof(currentText), "%.1f", displayedSpeed);
    snprintf(limitText, sizeof(limitText), "%.0f", speedLimit);
    snprintf(accelText, sizeof(accelText), "%.1f", accelTime);

    drawStatusMetricValue(52, 140, targetText, "", &Font48);
    drawStatusMetricValue(420, 140, currentText, "", &Font48);
    drawStatusMetricValue(52, 260, limitText, "", &Font48);
    drawStatusMetricValue(420, 260, accelText, "", &Font48);

    statusRegionDirty = true;
}

void drawOperationStatic()
{
    Paint_Clear(COLOR_BACKGROUND);
    drawHeaderStatic();
    fillRoundedPanel(26, 72, 778, 452, COLOR_PANEL_DARK);
    Paint_DrawString_EN(52, 94, "BASIC OPERATION", &Font24, COLOR_TEXT, COLOR_PANEL_DARK);
    
    /* NAME 显示 */
    drawStatusMetricStatic(52, 130, 708, 60, "NAME", COLOR_ACCENT);
    
    /* 运行状态与速度 */
    drawStatusMetricStatic(52, 210, 340, 100, "STATUS", COLOR_OK);
    drawStatusMetricStatic(420, 210, 340, 100, "SPEED", COLOR_ACCENT);
    
    /* 举升高度 */
    drawStatusMetricStatic(52, 330, 708, 100, "LIFT HEIGHT", COLOR_WARNING);
    
    drawFooter();
}

void drawOperationDynamic(float speed)
{
    /* 如果正在编辑，绘制编辑器（覆盖在静态背景上） */
    if (isEditingParam())
    {
        if (paramEditorDirty)
        {
            drawNumericEditor();
            paramEditorDirty = false;
        }
        return;
    }

    char speedText[16];
    char heightText[16];
    const char *runText = machineRunning ? "RUNNING" : "STOPPED";

    drawHeaderDynamic(speed);
    snprintf(speedText, sizeof(speedText), "%.1f", speed);
    snprintf(heightText, sizeof(heightText), "%.1f", liftTargetHeight);

    /* NAME */
    drawStatusMetricValue(52, 130, nameText, "", &Font24);
    
    /* STATUS 和 SPEED */
    drawStatusMetricValue(52, 210, runText, "", &Font48);
    drawStatusMetricValue(420, 210, speedText, "", &Font48);
    
    /* LIFT HEIGHT */
    drawStatusMetricValue(52, 330, heightText, "", &Font48);

    statusRegionDirty = true;
}

void drawStaticScreen()
{
    if (currentPage == PAGE_PARAM)
    {
        drawParamStatic();
    }
    else if (currentPage == PAGE_OPERATION)
    {
        drawOperationStatic();
    }
    else
    {
        drawStatusStatic();
    }
}

void drawDynamicScreen(float speed, float positionX, float positionY, float positionZ)
{
    if (currentPage == PAGE_PARAM)
    {
        drawParamDynamic(speed);
    }
    else if (currentPage == PAGE_OPERATION)
    {
        drawOperationDynamic(speed);
    }
    else
    {
        drawStatusDynamic(speed);
    }
}

void selectPage(uint8_t page)
{
    if (currentPage != page)
    {
        currentPage = page;
        screenDirty = true;
        dynamicDirty = true;

        /* 切换到 PARAM/OPER 页时向上位机请求该页参数 */
        if (page == PAGE_PARAM)
        {
            SP_SendReport("{\"rpt\":\"PARAM_REQ\"}");
        }
        else if (page == PAGE_OPERATION)
        {
            SP_SendReport("{\"rpt\":\"OPER_REQ\"}");
        }
    }
}

bool gt911WriteByte(uint8_t address, uint16_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool gt911ReadBytes(uint8_t address, uint16_t reg, uint8_t *buffer, uint8_t length)
{
    Wire.beginTransmission(address);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    if (Wire.endTransmission(false) != 0)
    {
        return false;
    }

    uint8_t received = Wire.requestFrom(address, length);
    if (received != length)
    {
        return false;
    }

    for (uint8_t index = 0; index < length; ++index)
    {
        buffer[index] = Wire.read();
    }

    return true;
}

void initTouch()
{
    IO_EXTENSION_Output(IO_EXTENSION_IO_1, 0);
    delay(20);
    IO_EXTENSION_Output(IO_EXTENSION_IO_1, 1);
    delay(120);

    uint8_t productId[4] = {0, 0, 0, 0};
    if (gt911ReadBytes(GT911_ADDR_PRIMARY, 0x8140, productId, sizeof(productId)))
    {
        touchAddress = GT911_ADDR_PRIMARY;
        touchReady = true;
    }
    else if (gt911ReadBytes(GT911_ADDR_SECONDARY, 0x8140, productId, sizeof(productId)))
    {
        touchAddress = GT911_ADDR_SECONDARY;
        touchReady = true;
    }

    if (touchReady)
    {
        gt911WriteByte(touchAddress, 0x814E, 0);
        Serial.printf("touch addr 0x%02X id %c%c%c%c\n", touchAddress, productId[0], productId[1], productId[2], productId[3]);
    }
    else
    {
        Serial.println("touch not found");
    }
}

bool readTouchPoint(uint16_t *x, uint16_t *y)
{
    if (!touchReady)
    {
        return false;
    }

    uint8_t status = 0;
    if (!gt911ReadBytes(touchAddress, 0x814E, &status, 1))
    {
        return false;
    }

    uint8_t pointCount = status & 0x0F;
    if ((status & 0x80) == 0 || pointCount == 0)
    {
        if ((status & 0x80) != 0)
        {
            gt911WriteByte(touchAddress, 0x814E, 0);
        }
        return false;
    }

    uint8_t data[8] = {0};
    bool ok = gt911ReadBytes(touchAddress, 0x8150, data, sizeof(data));
    gt911WriteByte(touchAddress, 0x814E, 0);
    if (!ok)
    {
        return false;
    }

    *x = static_cast<uint16_t>(data[1] << 8 | data[0]);
    *y = static_cast<uint16_t>(data[3] << 8 | data[2]);
    return true;
}

bool pageFromMappedPoint(uint16_t x, uint16_t y, ScreenPage *page)
{
    if (y < TAB_Y || y > TAB_Y + TAB_H)
    {
        return false;
    }

    if (x >= TAB_STATUS_X && x <= TAB_STATUS_X + TAB_W)
    {
        *page = PAGE_STATUS;
        return true;
    }
    if (x >= TAB_PARAM_X && x <= TAB_PARAM_X + TAB_W)
    {
        *page = PAGE_PARAM;
        return true;
    }
    if (x >= TAB_OPER_X && x <= TAB_OPER_X + TAB_W)
    {
        *page = PAGE_OPERATION;
        return true;
    }

    return false;
}

bool tryMappedPoint(int x, int y, ScreenPage *page, uint16_t *mappedX, uint16_t *mappedY)
{
    if (x < 0 || y < 0 || x >= EXAMPLE_LCD_H_RES || y >= EXAMPLE_LCD_V_RES)
    {
        return false;
    }

    uint16_t candidateX = static_cast<uint16_t>(x);
    uint16_t candidateY = static_cast<uint16_t>(y);
    if (!pageFromMappedPoint(candidateX, candidateY, page))
    {
        return false;
    }

    *mappedX = candidateX;
    *mappedY = candidateY;
    return true;
}

bool pageFromTouchPoint(uint16_t rawX, uint16_t rawY, ScreenPage *page, uint16_t *mappedX, uint16_t *mappedY)
{
    int x = rawX;
    int y = rawY;
    int maxX = EXAMPLE_LCD_H_RES - 1;
    int maxY = EXAMPLE_LCD_V_RES - 1;

    return tryMappedPoint(x, y, page, mappedX, mappedY) ||
           tryMappedPoint(maxX - x, y, page, mappedX, mappedY) ||
           tryMappedPoint(x, maxY - y, page, mappedX, mappedY) ||
           tryMappedPoint(maxX - x, maxY - y, page, mappedX, mappedY) ||
           tryMappedPoint(y, x, page, mappedX, mappedY) ||
           tryMappedPoint(maxX - y, x, page, mappedX, mappedY) ||
           tryMappedPoint(y, maxY - x, page, mappedX, mappedY) ||
           tryMappedPoint(maxX - y, maxY - x, page, mappedX, mappedY);
}

bool pointInRect(uint16_t x, uint16_t y, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

bool paramFieldFromMappedPoint(uint16_t x, uint16_t y, ParamEditField *field)
{
    /* 新布局: 全屏卡片 */
    /* TARGET SPEED: (52, 140) - (392, 240) */
    if (pointInRect(x, y, 52, 140, 392, 240))
    {
        *field = EDIT_TARGET_SPEED;
        return true;
    }
    /* CURRENT SPEED: (420, 140) - (760, 240) - 不可编辑 */
    /* SPEED LIMIT: (52, 260) - (392, 360) */
    if (pointInRect(x, y, 52, 260, 392, 360))
    {
        *field = EDIT_SPEED_LIMIT;
        return true;
    }
    /* ACCEL TIME: (420, 260) - (760, 360) */
    if (pointInRect(x, y, 420, 260, 760, 360))
    {
        *field = EDIT_ACCEL_TIME;
        return true;
    }
    return false;
}

bool tryMappedParamField(int x, int y, ParamEditField *field)
{
    if (x < 0 || y < 0 || x >= EXAMPLE_LCD_H_RES || y >= EXAMPLE_LCD_V_RES)
    {
        return false;
    }
    return paramFieldFromMappedPoint(static_cast<uint16_t>(x), static_cast<uint16_t>(y), field);
}

bool paramFieldFromTouchPoint(uint16_t rawX, uint16_t rawY, ParamEditField *field)
{
    int x = rawX;
    int y = rawY;
    int maxX = EXAMPLE_LCD_H_RES - 1;
    int maxY = EXAMPLE_LCD_V_RES - 1;

    return tryMappedParamField(x, y, field) ||
           tryMappedParamField(maxX - x, y, field) ||
           tryMappedParamField(x, maxY - y, field) ||
           tryMappedParamField(maxX - x, maxY - y, field) ||
           tryMappedParamField(y, x, field) ||
           tryMappedParamField(maxX - y, x, field) ||
           tryMappedParamField(y, maxY - x, field) ||
           tryMappedParamField(maxX - y, maxY - x, field);
}

bool operationPointFromMappedPoint(uint16_t x, uint16_t y, ParamEditField *field, bool *liftTo)
{
    /* NAME 卡片 (52, 130) - (760, 190) */
    if (pointInRect(x, y, 52, 130, 760, 190))
    {
        *field = EDIT_NAME;
        *liftTo = false;
        return true;
    }
    /* LIFT HEIGHT 全屏卡片 (52, 260) - (760, 360) */
    if (pointInRect(x, y, 52, 260, 760, 360))
    {
        *field = EDIT_LIFT_HEIGHT;
        *liftTo = false;
        return true;
    }
    return false;
}

bool tryMappedOperationPoint(int x, int y, ParamEditField *field, bool *liftTo)
{
    if (x < 0 || y < 0 || x >= EXAMPLE_LCD_H_RES || y >= EXAMPLE_LCD_V_RES)
    {
        return false;
    }
    return operationPointFromMappedPoint(static_cast<uint16_t>(x), static_cast<uint16_t>(y), field, liftTo);
}

bool operationPointFromTouchPoint(uint16_t rawX, uint16_t rawY, ParamEditField *field, bool *liftTo)
{
    int x = rawX;
    int y = rawY;
    int maxX = EXAMPLE_LCD_H_RES - 1;
    int maxY = EXAMPLE_LCD_V_RES - 1;

    return tryMappedOperationPoint(x, y, field, liftTo) ||
           tryMappedOperationPoint(maxX - x, y, field, liftTo) ||
           tryMappedOperationPoint(x, maxY - y, field, liftTo) ||
           tryMappedOperationPoint(maxX - x, maxY - y, field, liftTo) ||
           tryMappedOperationPoint(y, x, field, liftTo) ||
           tryMappedOperationPoint(maxX - y, x, field, liftTo) ||
           tryMappedOperationPoint(y, maxY - x, field, liftTo) ||
           tryMappedOperationPoint(maxX - y, maxY - x, field, liftTo);
}

char numericKeyFromMappedPoint(uint16_t x, uint16_t y)
{
    bool nameMode = (editingField == EDIT_NAME);
    const char keys[4][3] = {
        {'7', '8', '9'},
        {'4', '5', '6'},
        {'1', '2', '3'},
        {nameMode ? 'T' : '-', '0', nameMode ? '\0' : '.'}};

    /* 新布局: 全屏键盘 */
    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t col = 0; col < 3; col++)
        {
            if (keys[row][col] == '\0') continue;
            uint16_t keyX = 100 + col * 100;
            uint16_t keyY = 200 + row * 50;
            if (pointInRect(x, y, keyX, keyY, keyX + 80, keyY + 40))
            {
                return keys[row][col];
            }
        }
    }
    /* SLAM/QRCODE 切换键 (仅在名称编辑模式下) */
    if (editingField == EDIT_NAME)
    {
        if (pointInRect(x, y, 100, 350, 180, 390))
        {
            return 'T';
        }
    }
    /* DEL */
    if (pointInRect(x, y, 100, 406, 180, 448))
    {
        return '<';
    }
    /* OK */
    if (pointInRect(x, y, 200, 406, 280, 448))
    {
        return 'O';
    }
    /* ESC */
    if (pointInRect(x, y, 300, 406, 380, 448))
    {
        return 'E';
    }
    return '\0';
}

bool tryMappedNumericKey(int x, int y, char *key)
{
    if (x < 0 || y < 0 || x >= EXAMPLE_LCD_H_RES || y >= EXAMPLE_LCD_V_RES)
    {
        return false;
    }

    char foundKey = numericKeyFromMappedPoint(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
    if (foundKey == '\0')
    {
        return false;
    }
    *key = foundKey;
    return true;
}

bool numericKeyFromTouchPoint(uint16_t rawX, uint16_t rawY, char *key)
{
    int x = rawX;
    int y = rawY;
    int maxX = EXAMPLE_LCD_H_RES - 1;
    int maxY = EXAMPLE_LCD_V_RES - 1;

    return tryMappedNumericKey(x, y, key) ||
           tryMappedNumericKey(maxX - x, y, key) ||
           tryMappedNumericKey(x, maxY - y, key) ||
           tryMappedNumericKey(maxX - x, maxY - y, key) ||
           tryMappedNumericKey(y, x, key) ||
           tryMappedNumericKey(maxX - y, x, key) ||
           tryMappedNumericKey(y, maxY - x, key) ||
           tryMappedNumericKey(maxX - y, maxY - x, key);
}

void beginParamEdit(ParamEditField field)
{
    editingField = field;
    if (field == EDIT_TARGET_SPEED)
    {
        snprintf(editBuffer, sizeof(editBuffer), "%.1f", targetSpeed);
    }
    else if (field == EDIT_SPEED_LIMIT)
    {
        snprintf(editBuffer, sizeof(editBuffer), "%.1f", speedLimit);
    }
    else if (field == EDIT_ACCEL_TIME)
    {
        snprintf(editBuffer, sizeof(editBuffer), "%.1f", accelTime);
    }
    else if (field == EDIT_LIFT_HEIGHT)
    {
        snprintf(editBuffer, sizeof(editBuffer), "%.1f", liftTargetHeight);
    }
    else if (field == EDIT_NAME)
    {
        /* 解析当前 nameText 的前缀和数字部分 */
        if (strncmp(nameText, "QRCODE-", 7) == 0)
        {
            nameNavType = 1;
            strlcpy(editBuffer, nameText + 7, sizeof(editBuffer));
        }
        else
        {
            nameNavType = 0;
            const char *dash = strchr(nameText, '-');
            if (dash)
            {
                strlcpy(editBuffer, dash + 1, sizeof(editBuffer));
            }
            else
            {
                editBuffer[0] = '0';
                editBuffer[1] = '\0';
            }
        }
    }
    editBufferLen = strlen(editBuffer);
    paramEditorDirty = true;
    screenDirty = true;
    dynamicDirty = true;
}

void sendParamEditReport(float value)
{
    char rpt[96];
    snprintf(rpt, sizeof(rpt), "{\"rpt\":\"PARAM\",\"field\":\"%s\",\"val\":%.2f}", editFieldReportName(), value);
    SP_SendReport(rpt);
}

void sendLiftToReport()
{
    char rpt[96];
    snprintf(rpt, sizeof(rpt), "{\"rpt\":\"OPER\",\"cmd\":\"LIFT_TO\",\"height\":%.2f}", liftTargetHeight);
    SP_SendReport(rpt);
}

void appendEditChar(char key)
{
    if (editBufferLen >= sizeof(editBuffer) - 1)
    {
        return;
    }
    if (key == '.' && editingField == EDIT_NAME)
    {
        return;
    }
    if (key == '.' && strchr(editBuffer, '.') != nullptr)
    {
        return;
    }
    if (key == '-' && editingField == EDIT_NAME)
    {
        nameNavType = (nameNavType == 0) ? 1 : 0;
        paramEditorDirty = true;
        dynamicDirty = true;
        return;
    }
    if (key == '-' && editBufferLen != 0)
    {
        return;
    }
    editBuffer[editBufferLen++] = key;
    editBuffer[editBufferLen] = '\0';
    paramEditorDirty = true;
    dynamicDirty = true;  /* 触发重绘键盘 */
}

void finishParamEdit()
{
    if (editBufferLen == 0 || strcmp(editBuffer, ".") == 0 || strcmp(editBuffer, "-") == 0)
    {
        return;
    }

    float val = atof(editBuffer);
    float reportedVal = val;
    if (editingField == EDIT_TARGET_SPEED)
    {
        targetSpeed = constrain(val, 0.0f, speedLimit);
        reportedVal = targetSpeed;
        machineRunning = targetSpeed > 0.0f;
    }
    else if (editingField == EDIT_SPEED_LIMIT)
    {
        speedLimit = constrain(val, 1.0f, 240.0f);
        targetSpeed = constrain(targetSpeed, 0.0f, speedLimit);
        reportedVal = speedLimit;
    }
    else if (editingField == EDIT_ACCEL_TIME)
    {
        accelTime = constrain(val, 0.1f, 30.0f);
        reportedVal = accelTime;
    }
    else if (editingField == EDIT_LIFT_HEIGHT)
    {
        liftTargetHeight = constrain(val, 0.0f, 9999.0f);
        reportedVal = liftTargetHeight;
    }
    else if (editingField == EDIT_NAME)
    {
        const char *prefix = (nameNavType == 0) ? "SLAM" : "QRCODE";
        snprintf(nameText, sizeof(nameText), "%s-%s", prefix, editBuffer);
        char rpt[96];
        snprintf(rpt, sizeof(rpt), "{\"rpt\":\"PARAM\",\"field\":\"name\",\"val\":\"%s\"}", nameText);
        SP_SendReport(rpt);
        editingField = EDIT_NONE;
        paramEditorDirty = false;
        screenDirty = true;
        dynamicDirty = true;
        return;
    }

    if (editingField != EDIT_LIFT_HEIGHT)
    {
        sendParamEditReport(reportedVal);
    }

    editingField = EDIT_NONE;
    paramEditorDirty = false;
    screenDirty = true;
    dynamicDirty = true;
}

void handleNumericEditorTap(uint16_t x, uint16_t y)
{
    char key = '\0';
    if (!numericKeyFromTouchPoint(x, y, &key))
    {
        return;
    }

    if (key == 'T' && editingField == EDIT_NAME)
    {
        /* 切换 SLAM / QRCODE */
        nameNavType = (nameNavType == 0) ? 1 : 0;
        paramEditorDirty = true;
        dynamicDirty = true;
    }
    else if (key == '<')
    {
        editBuffer[0] = '\0';
        editBufferLen = 0;
        paramEditorDirty = true;
        dynamicDirty = true;
    }
    else if (key == 'O')
    {
        finishParamEdit();
    }
    else if (key == 'E')
    {
        editingField = EDIT_NONE;
        paramEditorDirty = false;
        screenDirty = true;
        dynamicDirty = true;
    }
    else
    {
        appendEditChar(key);
    }
}

void handleTap(uint16_t x, uint16_t y)
{
    if (isEditingParam())
    {
        handleNumericEditorTap(x, y);
        return;
    }

    /* 返回主界面按钮检测已移除 */

    ScreenPage page;
    uint16_t mappedX;
    uint16_t mappedY;
    if (pageFromTouchPoint(x, y, &page, &mappedX, &mappedY))
    {
        selectPage(page);
        return;
    }

    if (currentPage == PAGE_PARAM)
    {
        ParamEditField field;
        if (paramFieldFromTouchPoint(x, y, &field))
        {
            beginParamEdit(field);
        }
    }
    else if (currentPage == PAGE_OPERATION)
    {
        ParamEditField field;
        bool liftTo;
        if (!operationPointFromTouchPoint(x, y, &field, &liftTo))
        {
            return;
        }
        if (liftTo)
        {
            sendLiftToReport();
        }
        else
        {
            beginParamEdit(field);
        }
    }
}

ScreenPage nextPage()
{
    if (currentPage == PAGE_STATUS)
    {
        return PAGE_PARAM;
    }
    if (currentPage == PAGE_PARAM)
    {
        return PAGE_OPERATION;
    }
    return PAGE_STATUS;
}

ScreenPage previousPage()
{
    if (currentPage == PAGE_STATUS)
    {
        return PAGE_OPERATION;
    }
    if (currentPage == PAGE_PARAM)
    {
        return PAGE_STATUS;
    }
    return PAGE_PARAM;
}

void handleSwipe(uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY)
{
    int deltaX = static_cast<int>(endX) - static_cast<int>(startX);
    int deltaY = static_cast<int>(endY) - static_cast<int>(startY);
    int absDeltaX = abs(deltaX);
    int absDeltaY = abs(deltaY);

    if (absDeltaX < 120 && absDeltaY < 120)
    {
        return;
    }

    if (absDeltaX >= absDeltaY)
    {
        selectPage(deltaX < 0 ? nextPage() : previousPage());
    }
    else
    {
        selectPage(deltaY < 0 ? nextPage() : previousPage());
    }

}

void handleSwipeDuringTouch(uint16_t currentX, uint16_t currentY)
{
    if (!touchTracking || touchSwipeTriggered)
    {
        return;
    }

    int deltaX = static_cast<int>(currentX) - static_cast<int>(touchStartX);
    int deltaY = static_cast<int>(currentY) - static_cast<int>(touchStartY);
    int absDeltaX = abs(deltaX);
    int absDeltaY = abs(deltaY);

    if (absDeltaX < 100 && absDeltaY < 100)
    {
        return;
    }

    if (absDeltaX >= absDeltaY)
    {
        selectPage(deltaX < 0 ? nextPage() : previousPage());
    }
    else
    {
        selectPage(deltaY < 0 ? nextPage() : previousPage());
    }

    touchSwipeTriggered = true;
    gestureSwipeTriggered = true;
}

void handleFragmentedSwipe(uint16_t currentX, uint16_t currentY)
{
    if (touchSwipeTriggered)
    {
        return;
    }
    uint32_t now = millis();
    if (!gestureTracking || now - lastGestureMs > 650)
    {
        gestureTracking = true;
        gestureSwipeTriggered = false;
        gestureStartX = currentX;
        gestureStartY = currentY;
    }
    lastGestureMs = now;

    if (gestureSwipeTriggered)
    {
        return;
    }

    int deltaX = static_cast<int>(currentX) - static_cast<int>(gestureStartX);
    int deltaY = static_cast<int>(currentY) - static_cast<int>(gestureStartY);
    int absDeltaX = abs(deltaX);
    int absDeltaY = abs(deltaY);

    if (absDeltaX < 130 && absDeltaY < 130)
    {
        return;
    }

    if (absDeltaX >= absDeltaY)
    {
        selectPage(deltaX < 0 ? nextPage() : previousPage());
    }
    else
    {
        selectPage(deltaY < 0 ? nextPage() : previousPage());
    }

    touchSwipeTriggered = true;
    gestureSwipeTriggered = true;
}

void readTouchButtons()
{
    uint16_t x = 0;
    uint16_t y = 0;
    bool pressed = readTouchPoint(&x, &y);
    uint32_t now = millis();

    if (pressed)
    {
        lastTouchSeenMs = now;
        if (!touchTracking)
        {
            touchStartX = x;
            touchStartY = y;
            touchTracking = true;
            touchSwipeTriggered = false;
            gestureSwipeTriggered = false;
            gestureTracking = false;
            touchTapHandled = false;
            if (REPORT_TOUCH_EVENTS)
            {
                char rptDn[64];
                snprintf(rptDn, sizeof(rptDn), "{\"rpt\":\"TOUCH\",\"evt\":\"DOWN\",\"x\":%u,\"y\":%u}", x, y);
                SP_SendReport(rptDn);
            }
        }
        touchLastX = x;
        touchLastY = y;
        if (!isEditingParam())
        {
            handleFragmentedSwipe(x, y);
        }

        if (REPORT_TOUCH_EVENTS && !isEditingParam() && now - lastTouchLogMs > 250)
        {
            lastTouchLogMs = now;
            char rptMv[96];
            snprintf(rptMv, sizeof(rptMv), "{\"rpt\":\"TOUCH\",\"evt\":\"MOVE\",\"x\":%u,\"y\":%u,\"dx\":%d,\"dy\":%d}", x, y, static_cast<int>(x) - static_cast<int>(touchStartX), static_cast<int>(y) - static_cast<int>(touchStartY));
            SP_SendReport(rptMv);
        }

        if (!isEditingParam())
        {
            handleSwipeDuringTouch(x, y);
        }
    }
    else if (touchWasPressed && touchTracking)
    {
        if (now - lastTouchSeenMs < TOUCH_RELEASE_DEBOUNCE_MS)
        {
            return;
        }

        if (!isEditingParam())
        {
            handleFragmentedSwipe(touchLastX, touchLastY);
        }
        int deltaX = static_cast<int>(touchLastX) - static_cast<int>(touchStartX);
        int deltaY = static_cast<int>(touchLastY) - static_cast<int>(touchStartY);
        if (!touchTapHandled && !touchSwipeTriggered && abs(deltaX) < 45 && abs(deltaY) < 45)
        {
            handleTap(touchLastX, touchLastY);
        }
        if (REPORT_TOUCH_EVENTS)
        {
            char rptUp[64];
            snprintf(rptUp, sizeof(rptUp), "{\"rpt\":\"TOUCH\",\"evt\":\"UP\",\"x\":%u,\"y\":%u}", touchLastX, touchLastY);
            SP_SendReport(rptUp);
        }
        touchTracking = false;
        touchSwipeTriggered = false;
        touchTapHandled = false;
    }

    if (!pressed && gestureTracking && millis() - lastGestureMs > 650)
    {
        gestureTracking = false;
        gestureSwipeTriggered = false;
    }

    touchWasPressed = pressed;
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("init display");

    DEV_I2C_Init();
    IO_EXTENSION_Init();
    initTouch();
    IO_EXTENSION_Output(IO_EXTENSION_IO_3, 0);
    delay(20);
    IO_EXTENSION_Output(IO_EXTENSION_IO_3, 1);
    delay(120);
    waveshare_esp32_s3_rgb_lcd_init();
    waveshare_rgb_lcd_bl_on();

    frameBuffer = static_cast<uint16_t *>(heap_caps_malloc(
        EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    baseBuffer = static_cast<uint16_t *>(heap_caps_malloc(
        FRAME_BYTES,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    regionBuffer = static_cast<uint16_t *>(heap_caps_malloc(
        REGION_BUFFER_PIXELS * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (frameBuffer == nullptr || baseBuffer == nullptr || regionBuffer == nullptr)
    {
        Serial.println("display buffer allocation failed");
        while (true)
        {
            delay(1000);
        }
    }

    Paint_NewImage(reinterpret_cast<UBYTE *>(frameBuffer), EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, ROTATE_0, WHITE);
    Paint_SetScale(65);
    drawStaticScreen();
    memcpy(baseBuffer, frameBuffer, FRAME_BYTES);
    waveshare_rgb_lcd_display(reinterpret_cast<uint8_t *>(frameBuffer));
    Serial.println("display ready");
    SP_Init(Serial);
}

void loop()
{
    SP_Process();
    while (SP_HasPendingFrame())
    {
        uint16_t len = 0;
        const uint8_t *frame = SP_GetFrame(&len);
        Serial.write(frame, len);
        SP_ClearPending();
    }
    readTouchButtons();
    while (SP_HasPendingFrame())
    {
        uint16_t len = 0;
        const uint8_t *frame = SP_GetFrame(&len);
        Serial.write(frame, len);
        SP_ClearPending();
    }

    uint32_t now = millis();
    if (now - lastFrameMs < FRAME_INTERVAL_MS)
    {
        return;
    }
    lastFrameMs = now;

    /* 速度平滑过渡 */
    float speedStep = 0.15f / accelTime;
    displayedSpeed += (targetSpeed - displayedSpeed) * constrain(speedStep, 0.02f, 0.8f);

    /* 检测显示值是否真正变化 (避免心跳包导致无意义重绘) */
    float roundedSpeed = round(displayedSpeed * 10.0f) / 10.0f;
    if (roundedSpeed != lastDisplayedSpeedRounded)
    {
        lastDisplayedSpeedRounded = roundedSpeed;
        dynamicDirty = true;
    }

    bool needFullUpload = screenDirty || isEditingParam();

    if (screenDirty)
    {
        drawStaticScreen();
        memcpy(baseBuffer, frameBuffer, FRAME_BYTES);
        screenDirty = false;
    }

    if (dynamicDirty)
    {
        drawDynamicScreen(displayedSpeed, positionX, positionY, positionZ);
        dynamicDirty = false;
        /* PARAM/OPER 页的参数值不在 GAUGE/STATUS 区域内, 必须全屏上传 */
        if (currentPage == PAGE_PARAM || currentPage == PAGE_OPERATION)
        {
            needFullUpload = true;
        }
    }

    /* 统一上传: 静态变化或编辑模式时全屏上传, 否则按区域上传 */
    if (needFullUpload)
    {
        waveshare_rgb_lcd_display(reinterpret_cast<uint8_t *>(frameBuffer));
        gaugeRegionDirty = false;
        statusRegionDirty = false;
    }
    else
    {
        if (gaugeRegionDirty)
        {
            uploadRegion(GAUGE_REGION_X, GAUGE_REGION_Y, GAUGE_REGION_W, GAUGE_REGION_H);
            gaugeRegionDirty = false;
        }
        if (statusRegionDirty)
        {
            uploadRegion(STATUS_REGION_X, STATUS_REGION_Y, STATUS_REGION_W, STATUS_REGION_H);
            statusRegionDirty = false;
        }
    }
}