# ESP32-S3-Touch-LCD-4.3C

URL: https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/

[![ESP32-S3-Touch-LCD-4.3C](https://docs.waveshare.net/assets/images/Product-Example-1-9f49fe0d63f3e31a00c84732640f3803.webp)](https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-4.3C.htm)

ESP32-S3-Touch-LCD-4.3C 是微雪 (Waveshare) 低成本高性能开发板，集成 2.4GHz WiFi、BLE5、大容量 Flash 与 PSRAM，板载 4.3 英寸电容触摸 LCD，可流畅运行 LVGL GUI，搭载 ES8311 音频编解码器与 ES7210 四通道 ADC，支持语音采集与播放，并提供丰富 I2C、宽电压 IO 等接口，适用于 IoT、移动设备与智能家居等应用。

**如果你在寻找：**

- **使用 AI 工具编程** ：点击 **发给 AI** 并选择 AI 工具，询问与本页面相关的问题；也可以选择 **复制为 Markdown** ，将页面内容复制并粘贴到 AI 工具中辅助编程。
- **原理图、规格书和示例代码** ：参见 [相关资料](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/Resources-And-Documents)
- **出厂固件** ：参见 [出厂固件使用](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/FactoryFirmWare)
- **Arduino 开发** ：参见 [Arduino 开发](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/Arduino)
- **ESP-IDF 开发** ：参见 [ESP-IDF 开发](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/ESP-IDF)
- **问题排查** ：参见 [产品 FAQ](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/FAQ) 或联系 [技术支持](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/Technical-Support)

| SKU | 产品 |
| --- | --- |
| 33799 | ESP32-S3-Touch-LCD-4.3C |
| 33630 | ESP32-S3-Touch-LCD-4.3C-BOX |

## 产品特性

- 搭载 **ESP32-S3R8** 高性能 Xtensa® 32 位 LX7 双核处理器，主频达 240 MHz
- 支持 2.4 GHz Wi-Fi (802.11 b/g/n) 和 Bluetooth® 5 (LE)，并集成板载天线
- 内置 512KB 的 SRAM 和 384KB ROM，叠封 8MB PSRAM 和外接 16MB Flash 存储
- 板载 4.3 英寸宽电容触摸屏，800 × 480 分辨率，65K 彩色
- 支持 I2C 接口控制电容触摸，5 点触控，支持中断

## 板载资源

![ESP32-S3-Touch-LCD-4.3C 板载资源示例 1](https://docs.waveshare.net/assets/images/Product-Example-2-4c92d7f0579d3510573be548ef8d5724.webp)

- 板载 PCF85063 RTC 实时时钟芯片，可实现掉电保持时间和闹钟功能。
- 板载 4.3 英寸 800×480 电容触摸 LCD 屏，支持 LVGL 图形界面显示。
- 板载隔离 IO 接口，提供 2 路数字输入（DI0、DI1）和 2 路数字输出（DO0、DO1），可通过 IO_EXTENSION 与外部开关量设备安全隔离交互。
- 板载 TF 卡座，采用 SD_MMC 接口访问 SD 卡，用于存储图片、音乐等资源。
- 板载 ES8311 音频功放编解码器与 ES7210 四通道音频 ADC，配合扬声器和麦克风接口。
- 板载 USB Type-C 接口，用于固件下载、串口日志打印及 USB 相关功能调试，是 Arduino 和 ESP-IDF 示例的主要通信与下载通道。
- 板载 BOOT、RST 功能按钮，可快捷进入下载模式并复位运行不同固件。

## 接口说明

在使用 ESP32-S3-Touch-LCD-4.3C 时，需要了解不同外设的硬件连接

**ESP32-S3 / IO EXTENSION 与 LCD 引脚对照表（点击展开）**
| ESP32-S3 | LCD | 简介 |
| --- | --- | --- |
| **ESP32-S3 → LCD RGB 接口** | **—** | **直连引脚定义** |
| GPIO0 | G3 | 绿色数据第三位 |
| GPIO1 | R3 | 红色数据第三位 |
| GPIO2 | R4 | 红色数据第四位 |
| GPIO3 | VSYNC | 竖向同步信号 |
| GPIO5 | DE | 数据使能信号 |
| GPIO7 | PCLK | 时钟信号 |
| GPIO10 | B7 | 蓝色第七位 |
| GPIO14 | B3 | 蓝色第三位 |
| GPIO17 | B6 | 蓝色第六位 |
| GPIO18 | B5 | 蓝色第五位 |
| GPIO21 | G7 | 绿色第七位 |
| GPIO38 | B4 | 蓝色第四位 |
| GPIO39 | G2 | 绿色第二位 |
| GPIO40 | R7 | 红色第七位 |
| GPIO41 | R6 | 红色第六位 |
| GPIO42 | R5 | 红色第五位 |
| GPIO45 | G4 | 绿色第四位 |
| GPIO46 | HSYNC | 横向同步信号 |
| GPIO47 | G6 | 绿色第六位 |
| GPIO48 | G5 | 绿色第五位 |
| **IO EXTENSION** | **—** | **IO 扩展控制** |
| EXIO2 | DISP | 背光使能引脚 |
| EXIO_PWM | BL_PWM | 背光调节引脚 |

**ESP32-S3 / IO EXTENSION 与 Touch 引脚对照表（点击展开）**
| ESP32-S3 | Touch | 简介 |
| --- | --- | --- |
| **ESP32-S3 → Touch 接口** | **—** | **直连引脚定义** |
| GPIO4 | G3 | 触摸中断引脚 |
| GPIO8 | R3 | I2C 数据引脚 |
| GPIO9 | R4 | I2C 时钟引脚 |
| **IO EXTENSION** | **—** | **IO 扩展控制** |
| EXIO1 | DISP | 触摸复位引脚 |

**ESP32-S3 与 USB 引脚对照表（点击展开）**
| ESP32-S3 | USB | 简介 |
| --- | --- | --- |
| **ESP32-S3 → Touch 接口** | **—** | **直连引脚定义** |
| GPIO19 | USB_DN | 数据线 D- |
| GPIO20 | USB_DP | 数据线 D+ |

**ESP32-S3 / IO EXTENSION 与 TF 卡 引脚对照表（点击展开）**
| ESP32-S3 | TF | 简介 |
| --- | --- | --- |
| **ESP32-S3 → TF 接口** | **—** | **直连引脚定义** |
| GPIO11 | MOSI | TF 卡输入引脚 |
| GPIO12 | SCK | TF 卡时钟引脚 |
| GPIO13 | MISO | TF 卡输出引脚 |
| **IO EXTENSION** | **—** | **IO 扩展控制** |
| EXIO4 | SD_CS | TF 卡使能引脚 |

**ESP32-S3 / IO EXTENSION 与 RTC 引脚对照表（点击展开）**
| ESP32-S3 / IO EXT | RTC | 简介 |
| --- | --- | --- |
| **RTC 接口** | **—** | **直连引脚定义** |
| GPIO8 | RTC_SDA | RTC 数据引脚 |
| GPIO9 | RTC_SCL | RTC 时钟引脚 |
| **IO EXTENSION** | **—** | **IO 扩展控制** |
| INT | RTC_INT | RTC 中断引脚 |

**ESP32-S3 / IO EXTENSION 与 I2C 引脚对照表（点击展开）**
| ESP32-S3 | I2C | 简介 |
| --- | --- | --- |
| **I2C 接口** | **—** | **直连引脚定义** |
| GPIO8 | SDA | I2C 数据引脚 |
| GPIO9 | SCL | I2C 时钟引脚 |

**ESP32-S3 / IO EXTENSION 与 音频编解码 引脚对照表（点击展开）**
| ESP32-S3 / IO EXT | SPEAKER / MIC | 简介 |
| --- | --- | --- |
| **音频编解码接口** | **—** | **直连引脚定义** |
| GPIO6 | I2S_MCLK | I2S 主时钟 |
| GPIO8 | Audio_SDA | I2C 数据线 |
| GPIO9 | Audio_SCL | I2C 时钟线 |
| GPIO15 | I2S_DSDIN | I2S 数据输入 |
| GPIO16 | I2S_LRCK | 帧同步/左右声道选择 |
| GPIO43 | I2S_ASDOUT | I2S 数据输出 |
| GPIO44 | I2S_SCLK | I2S 串行时钟 |
| **IO EXTENSION** | **—** | **IO 扩展控制** |
| EXIO4 | PA_CTRL | 功放控制引脚 |

**ESP32-S3 / IO EXTENSION 与 隔离 IO 引脚对照表（点击展开）**
| IO EXTENSION | DI / DO | 简介 |
| --- | --- | --- |
| **隔离 IO 接口** | **—** | **IO 扩展控制** |
| EXIO0 | DI0 | 数字输入 0 |
| EXIO5 | DI1 | 数字输入 1 |
| EXIO6 | DO0 | 数字输出 0 |
| EXIO7 | DO1 | 数字输出 1 |

- **MX1.25 电池接口** ：开发板使用充放电管理芯片 CS8501，可将单节锂电池升压到 5 V，目前充电电流为 580 mA，可更换 R45 电阻修改充电电流，详情请查阅 [ESP32-S3-Touch-LCD-4.3C 原理图](https://gitee.com/waveshare/esp32-s3-touch-lcd-4.3-c/raw/main/hardware/schematics/ESP32-S3-Touch-LCD-4.3C-Schematics.pdf)

## 产品尺寸

### 不带外壳

![ESP32-S3-Touch-LCD-4.3C 板载资源示例2](https://docs.waveshare.net/assets/images/Product-Example-3-1d81dfcd010333e2d7a801b9ee30f159.webp)

### 带外壳

![ESP32-S3-Touch-LCD-4.3C 板载资源示例2](https://docs.waveshare.net/assets/images/Product-Example-4-34b9e7e7862afd8b0058300abaec3a00.webp)

## 开发方式

ESP32-S3-Touch-LCD-4.3C 支持 Arduino IDE 和 ESP-IDF 两种开发框架，为开发者提供灵活的选择，您可以根据项目需求和个人偏好选择合适的开发工具。

Arduino 通常更容易上手，对初学者和爱好者更加友好。ESP-IDF 提供更完整的开发工具链和更细粒度的系统控制，适合复杂项目或对性能有较高要求的应用。

- **Arduino IDE** 是一款便捷灵活、易于上手的开源电子原型平台。无需太多基础知识，简单学习后即可快速开发。Arduino 拥有庞大的全球用户社区，提供海量开源代码、项目示例和教程，以及丰富的库资源，封装了复杂功能，让开发者能够快速实现各种功能。您可以参考 **[Arduino IDE 开发环境搭建教程](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/Arduino)** 完成初始设置，教程中同时提供了相关示例程序供参考。
- **ESP-IDF** 全称 Espressif IoT Development Framework，是乐鑫科技为 ESP 系列芯片推出的专业开发框架。它基于 C 语言开发，包含编译器、调试器、烧录工具等，支持命令行或集成开发环境（如 Visual Studio Code 配合 Espressif IDF 插件）开发，插件提供代码导航、项目管理、调试等功能。我们推荐使用 VS Code 进行开发，具体配置过程可参考 **[ESP-IDF (VS Code) 开发环境搭建教程](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4.3C/ESP-IDF)** ，教程中同时提供了相关示例程序供参考。

[文档反馈](https://wj.qq.com/s2/26077836/3c81/?q-2-rR1G=https%3A%2F%2Fdocs.waveshare.net%2FESP32-S3-Touch-LCD-4.3C%2F)