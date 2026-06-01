# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 ESP-IDF v5.5.4 的 ESP32-S3 聊天设备项目，使用 ESP32-S3-Touch-LCD-1.9 开发板，包含 SH8601 LCD 显示屏驱动、CST816T 触摸驱动和聊天 UI 功能。

## 开发板资料

- 开发板型号: ESP32-S3-Touch-LCD-1.9
- 详细文档: `docs/ESP32-S3-Touch-LCD-1.9.md`
- LVGL 集成指南: `docs/LVGL_Integration.md`
- 项目总结: `docs/Project_Summary.md`
- 显示驱动: SH8601 (170×320, RGB565, SPI, 使用官方 esp_lcd_sh8601 组件)
- 触摸芯片: CST816T (I2C 地址 0x15, 单点触摸)
- UI 框架: LVGL v9.1

## 构建命令

### 使用构建脚本 (推荐)

```bash
# 显示帮助
./build.sh help

# 编译项目
./build.sh build

# 烧录固件
./build.sh flash /dev/tty.usbmodem0

# 烧录并监控
./build.sh flash-monitor

# 清理构建产物
./build.sh clean

# 打开菜单配置
./build.sh menuconfig
```

### 直接使用 idf.py

```bash
# 设置目标芯片（仅首次）
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控（串口根据系统调整，Windows 为 COM5）
idf.py -p /dev/tty.usbmodem* flash monitor

# 仅烧录
idf.py -p /dev/tty.usbmodem* flash

# 清理构建产物
idf.py fullclean

# 菜单配置
idf.py menuconfig
```

## 架构

**入口点**: `main/main.c` → `app_main()`

**模块结构**:
- `main/main.c` - 程序入口，LCD 初始化，任务创建
- `main/display/lv_port_disp.c` - LVGL 显示端口（使用 esp_lcd API）
- `main/display/backlight_manager.c` - 背光管理器（亮度调节、自动休眠、渐变动效）
- `main/display/lcd_config.h` - LCD 配置常量
- `main/input/lvgl_touch.c` - LVGL 触摸输入适配层
- `main/ui/lvgl_chat_ui.c` - LVGL 聊天界面（使用 LVGL 控件）
- `main/ui/lvgl_widgets.c` - LVGL 自定义控件
- `main/comm/uart_comm.c` - UART 通信协议（JSON 命令解析）
- `components/esp_lcd_sh8601/` - 官方 SH8601 LCD 驱动组件
- `components/i2c_bsp/` - I2C 主机驱动封装
- `components/esp_touch/` - CST816T 触摸控制器驱动（I2C 地址 0x15）
- `components/lvgl/` - LVGL 图形库组件

**数据流**: UART 接收 JSON 命令 → 命令队列 → UI 更新 → LCD 刷新

**硬件引脚**:

| GPIO      | LCD           | SD Card      | IMU           | UART       | I2C     | KEY_IO   | Other        |
| :-------- | :------------ | :----------- | :------------ | :--------- | :------ | :-------- | :----------- |
| IO0       | —             | —            | —             | —          | —       | **BOOT0** | IO0          |
| IO1       | —             | —            | —             | —          | —       | —         | IO1          |
| IO2       | —             | —            | —             | —          | —       | —         | IO2          |
| IO3       | —             | —            | —             | —          | —       | —         | IO3          |
| IO4       | —             | —            | —             | —          | —       | —         | **BAT_ADC**  |
| IO5       | —             | —            | —             | —          | —       | —         | IO5          |
| IO6       | —             | —            | —             | —          | —       | —         | IO6          |
| IO7       | —             | —            | **IMU_INT2**  | —          | —       | —         | IO7          |
| IO8       | —             | —            | **IMU_INT1**  | —          | —       | —         | —            |
| IO9       | **LCD_RST**   | —            | —             | —          | —       | —         | —            |
| IO10      | **LCD_CLK**   | —            | —             | —          | —       | —         | —            |
| IO11      | **LCD_DC**    | —            | —             | —          | —       | —         | —            |
| IO12      | **LCD_CS**    | —            | —             | —          | —       | —         | —            |
| IO13      | **LCD_DIN**   | —            | —             | —          | —       | —         | —            |
| IO14      | **LCD_BL**    | —            | —             | —          | —       | —         | —            |
| IO15      | —             | —            | —             | —          | —       | —         | IO15         |
| IO16      | —             | —            | —             | —          | —       | —         | IO16         |
| IO17      | **TP_RESET**  | —            | —             | —          | —       | —         | IO17         |
| IO18      | —             | —            | —             | —          | —       | —         | IO18         |
| IO19      | —             | —            | —             | —          | —       | —         | IO19         |
| IO20      | —             | —            | —             | —          | —       | —         | IO20         |
| IO21      | **TP_INT**    | —            | —             | —          | —       | —         | IO21         |
| IO33      | —             | —            | —             | —          | —       | —         | IO33         |
| IO34      | —             | —            | —             | —          | —       | —         | IO34         |
| IO35      | —             | —            | —             | —          | —       | —         | IO35         |
| IO36      | —             | —            | —             | —          | —       | —         | IO36         |
| IO37      | —             | —            | —             | —          | —       | —         | IO37         |
| IO38      | —             | —            | —             | —          | —       | —         | IO38         |
| IO39      | —             | **SD_MOSI**  | —             | —          | —       | —         | IO39         |
| IO40      | —             | **SD_MISO**  | —             | —          | —       | —         | IO40         |
| IO41      | —             | **SD_CLK**   | —             | —          | —       | —         | IO41         |
| IO42      | —             | —            | —             | —          | —       | —         | IO42         |
| IO43      | —             | —            | —             | **U0_TX**  | —       | —         | IO43         |
| IO44      | —             | —            | —             | **U0_RX**  | —       | —         | IO44         |
| IO45      | —             | —            | —             | —          | —       | —         | IO45         |
| IO46      | —             | —            | —             | —          | —       | —         | IO46         |
| IO47      | **TP_SDA**    | —            | **IMU_SDA**   | —          | **SDA** | —         | IO47         |
| IO48      | **TP_SCL**    | —            | **IMU_SCL**   | —          | **SCL** | —         | IO48         |
| **RESET** | —             | —            | —             | —          | —       | **RESET** | **CHP_UP**   |

## 配置文件

- `sdkconfig.defaults` - 基础配置（纳入版本控制）
- `sdkconfig` - 生成的完整配置（已 gitignore）
- `.clangd` - 配置为 ESP 平台，编译数据库位于 `build/`

## 依赖

- `espressif/led_strip: ^3.0.1`（通过 ESP-IDF 组件管理器）

## 开发注意事项

- FreeRTOS 节拍率: 1000Hz
- CPU 频率: 240MHz
- LCD 使用 esp_lcd_sh8601 官方组件，通过 esp_lcd API 操作
- UART 协议使用 JSON 格式，支持 status/chat/clear/progress 四种命令类型
- LVGL 版本: 9.1，通过 ESP-IDF 组件管理器集成
- LVGL 显示缓冲: 单缓冲模式，40 行部分渲染
- LVGL 处理频率: 100Hz (10ms 间隔)
- LCD 颜色配置排查: LVGL 输出 RGB565，`LV_COLOR_16_SWAP` 只处理 16 位像素字节序；`rgb_ele_order` 处理 R/B 通道顺序；`esp_lcd_panel_invert_color()` 处理面板反相。若 RED 显示为黄色，通常是 `rgb_ele_order` 配成 BGR 后红色先被解释为蓝色，再被反相成黄色；本板实测 RED 正确显示需要使用 `LCD_RGB_ELEMENT_ORDER_RGB`，并根据屏幕极性确认 `invert_color` 状态。
- 触摸坐标映射: 原始坐标 (0-4095) → 屏幕坐标 (0-169, 0-319)
- 背光控制: LEDC PWM，低电平亮（duty=0 最亮，duty=255 最暗），由 `backlight_manager` 模块统一管理，支持 0-255 亮度调节、自动休眠/唤醒、渐变动效
- 背光文档: `docs/Backlight_Manager.md`
- 初始化顺序: LCD → LVGL → I2C → 触摸 → UI → backlight_manager
