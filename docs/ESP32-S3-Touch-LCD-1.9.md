# ESP32-S3-Touch-LCD-1.9 开发板文档

## 概述

ESP32-S3-Touch-LCD-1.9 是一款基于 ESP32-S3 的开发板，配备 1.9 英寸触摸屏，适用于人机交互界面开发。

## 硬件规格

| 参数 | 规格 |
|------|------|
| 主控芯片 | ESP32-S3 |
| Flash | 16MB |
| PSRAM | 8MB (八线模式 @ 80MHz) |
| 屏幕尺寸 | 1.9 英寸 |
| 分辨率 | 170 × 320 像素 |
| 显示驱动 | ST7789V2 |
| 触摸芯片 | CST816T |
| 通信接口 | SPI (显示), I2C (触摸) |

## 显示驱动 - ST7789V2

ST7789V2 是一款 262K 色 TFT LCD 驱动芯片，支持 RGB565 颜色格式。

### 关键特性

- 支持 170×320 分辨率
- RGB565 颜色格式（16 位/像素）
- SPI 接口通信
- 支持硬件旋转（0°/90°/180°/270°）

### 引脚连接

| 信号 | GPIO | 说明 |
|------|------|------|
| LCD_RST | IO9 | 复位引脚 |
| LCD_CLK | IO10 | SPI 时钟 |
| LCD_DC | IO11 | 数据/命令选择 |
| LCD_CS | IO12 | 片选信号 |
| LCD_DIN | IO13 | SPI MOSI |
| LCD_BL | IO14 | 背光控制 |

### Arduino 示例库

- **GFX Library for Arduino**: 提供图形绘制 API
  - GitHub: https://github.com/moononournation/Arduino_GFX

## 触摸芯片 - CST816T

CST816T 是一款电容式触摸控制芯片，支持 I2C 通信，可检测单点触摸。

### 关键特性

- I2C 地址: 0x15
- 支持单点触摸检测
- 12 位坐标精度 (0-4095)
- 中断输出（可选）

### 引脚连接

| 信号 | GPIO | 说明 |
|------|------|------|
| TP_RESET | IO17 | 复位引脚 |
| TP_INT | IO21 | 中断信号 |
| TP_SDA | IO47 | I2C 数据线 |
| TP_SCL | IO48 | I2C 时钟线 |

### 寄存器格式

从地址 0x00 读取 7 字节：

| 偏移 | 内容 | 说明 |
|------|------|------|
| [0] | 模式 | 工作模式 |
| [1] | 保留 | - |
| [2] | 触摸点数 | 0=释放, 1=按下 |
| [3] | X 高 4 位 | 低 4 位有效 |
| [4] | X 低 8 位 | - |
| [5] | Y 高 4 位 | 低 4 位有效 |
| [6] | Y 低 8 位 | - |

坐标计算: `x = (tp[3] & 0x0F) << 8 | tp[4]`

### Arduino 示例库

- **Arduino_DriveBus**: 提供触摸驱动封装

## I2C 总线配置

触摸和 IMU 共享 I2C 总线 0:

| 参数 | 值 |
|------|-----|
| I2C 端口 | I2C_NUM_0 |
| SDA | GPIO 47 |
| SCL | GPIO 48 |
| 时钟频率 | 200 kHz |
| 内部上拉 | 启用 |

## 其他硬件接口

### SD 卡 (SPI)

| 信号 | GPIO |
|------|------|
| SD_MOSI | IO39 |
| SD_MISO | IO40 |
| SD_CLK | IO41 |

### IMU (六轴传感器)

| 信号 | GPIO | 说明 |
|------|------|------|
| IMU_INT1 | IO8 | 中断 1 |
| IMU_INT2 | IO7 | 中断 2 |
| IMU_SDA | IO47 | I2C 数据 (共享) |
| IMU_SCL | IO48 | I2C 时钟 (共享) |

### UART 通信

| 信号 | GPIO | 说明 |
|------|------|------|
| U0_TX | IO43 | 串口发送 |
| U0_RX | IO44 | 串口接收 |

### 电源监测

| 信号 | GPIO | 说明 |
|------|------|------|
| BAT_ADC | IO4 | 电池电压检测 |

## ESP-IDF 驱动开发

本项目使用 ESP-IDF 框架开发，相关驱动代码位于:

- `main/display/sh8601.c` - LCD 驱动 (当前使用 SH8601 命令集，需适配 ST7789V2)
- `main/ui/chat_ui.c` - 聊天界面 UI
- `components/i2c_bsp/` - I2C 主机驱动
- `components/esp_touch/` - 触摸控制器驱动

### 驱动适配说明

当前代码基于 SH8601 驱动芯片，如需适配 ST7789V2:

1. 修改 `main/display/sh8601.c` 中的初始化命令序列
2. 调整像素格式和分辨率配置
3. 更新背光控制逻辑

## 参考资料

- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ST7789V2 数据手册](https://www.sitronix.com.tw/en/products/display-driver-ic/)
- [CST816T 数据手册](http://www.champion-micro.com/)
