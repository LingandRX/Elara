# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

基于 ESP-IDF v6.0.2 的 ESP32-S3 桌面宠物设备项目（Elara Buddy，从 claude-desktop-buddy 迁移），使用 ESP32-S3-Touch-LCD-1.9 开发板，包含 ST7789 LCD 显示屏驱动、CST816T 触摸驱动、Buddy 宠物状态机 UI、WiFi/TCP 网络通信、SNTP 时间同步、电池监测和 LittleFS 存储等功能。

## 开发板资料

- 开发板型号: ESP32-S3-Touch-LCD-1.9
- 详细文档: `docs/ESP32-S3-Touch-LCD-1.9.md`
- 项目总结: `docs/Project_Summary.md`
- 上位机通信协议: `docs/上位机通信协议.md`
- 显示驱动: ST7789 (170×320, RGB565, SPI, 使用 ESP-IDF 官方 esp_lcd_panel_st7789 驱动)
- 触摸芯片: CST816T (I2C 地址 0x15, 单点触摸, 直接输出屏幕像素坐标 0~170/0~320)
- UI 框架: LVGL v8.4.0 (通过组件管理器集成, `lvgl/lvgl: ^8.4.0`)

## 架构

**入口点**: `main/main.c` → `app_main()`

**模块结构**:
- `main/main.c` - 程序入口，LCD/系统初始化，主循环（状态机 + BOOT 键处理）
- `main/display/lv_port_disp.c` - LVGL 显示端口（使用 esp_lcd API + LEDC PWM 背光）
- `main/display/backlight_manager.c` - 背光管理器（亮度调节、渐变动效）
- `main/display/lcd_config.h` - LCD 配置常量（含 BOOT_KEY_PIN）
- `main/input/lvgl_touch.c` - LVGL 触摸输入适配层（含滑动检测）
- `main/ui/buddy/buddy_ui.c` - Buddy 主界面 UI（HUD/时钟/菜单/设置/审批弹窗）
- `main/ui/buddy/buddy_anim.c` - Buddy 角色动画（ASCII/GIF 物种）
- `main/ui/pet_ui.c` - Petdex 动画页面
- `main/ui/pet_anim.c` - 宠物动画精灵图加载
- `main/ui/lvgl_chat_ui.c` - LVGL 聊天界面（兼容遗留）
- `main/ui/lvgl_widgets.c` - LVGL 自定义控件
- `main/buddy/buddy_state.c` - Buddy 核心状态机（7 种 Persona 状态、Claude 会话数据、审批提示）
- `main/buddy/buddy_stats.c` - Buddy 宠物统计（等级/心情/能量等，NVS 持久化）
- `main/comm/uart_comm.c` - UART 通信协议（JSON 命令解析）
- `main/comm/tcp_server.c` - TCP Server（端口 8080，网络指令接收）
- `main/comm/sntp_sync.c` - SNTP 网络时间同步
- `main/comm/xfer.c` - 文件上传传输（UART/TCP 共享）
- `main/wifi_manager.c` - WiFi 管理（连接/配置持久化/SNTP 触发）
- `main/storage_manager.c` - LittleFS 存储管理（挂载 /spiffs，宠物精灵图目录）
- `main/battery/battery_monitor.c` - 电池监测（ADC 电压 → 百分比/充电状态）
- `components/i2c_bsp/` - I2C 主机驱动封装（新 master driver API）
- `components/esp_touch/` - CST816T 触摸控制器驱动（I2C 地址 0x15）
- `managed_components/` - 组件管理器集成（lvgl / littlefs / cjson）

**数据流**: UART/TCP 接收 JSON 命令 → 命令队列 → UI 更新 → LCD 刷新；WiFi 获取 IP 后触发 SNTP 时间同步

**任务结构**:
- `lvgl_handler` - LVGL 渲染任务（基于 lv_task_handler 返回间隔动态调度）
- `uart_rx` - UART 接收任务
- `cmd_proc` - 命令处理任务（Petdex 等命令分发）
- 主循环直接内联在 `app_main` 中（Buddy 状态机 + BOOT 键轮询，LTPO-lite 动态帧率）

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
- `partitions.csv` - 自定义分区表（factory app 8MB + storage LittleFS 分区）
- `.clangd` - 配置为 ESP 平台，编译数据库位于 `build/`

## 依赖

- `lvgl/lvgl: ^8.4.0`（LVGL 图形库，通过 ESP-IDF 组件管理器）
- `joltwallet/littlefs: ^1.14.0`（LittleFS 文件系统）
- `espressif/cjson: ^1.7.19`（JSON 解析，UART/TCP 协议）

## 开发注意事项

- FreeRTOS 节拍率: 1000Hz
- CPU 频率: 160MHz
- LCD 使用 ESP-IDF 官方 esp_lcd_panel_st7789 驱动，通过 esp_lcd API 操作
- UART 协议使用 JSON 格式，支持 status/chat/cmd(clear)/progress/petdex/upload 等命令类型
- LVGL 版本: 8.4，通过 ESP-IDF 组件管理器集成
- LVGL 显示缓冲: 双缓冲模式，每缓冲 160 行（LVGL_BUF_LINES=160），DMA 内存分配
- LVGL tick: 2ms 周期（esp_timer 实现，比 FreeRTOS 任务更精确）
- LCD 颜色配置排查: LVGL 输出 RGB565，`LV_COLOR_16_SWAP` 只处理 16 位像素字节序；`rgb_ele_order` 处理 R/B 通道顺序；`esp_lcd_panel_invert_color()` 处理面板反相。若 RED 显示为黄色，通常是 `rgb_ele_order` 配成 BGR 后红色先被解释为蓝色，再被反相成黄色；本板实测 RED 正确显示需要使用 `LCD_RGB_ELEMENT_ORDER_RGB` + `data_endian = LCD_RGB_DATA_ENDIAN_BIG`，并根据屏幕极性确认 `invert_color` 状态。
- 触摸坐标: CST816T 直接输出屏幕像素坐标 (0-169, 0-319)，无需缩放映射；触摸滑动检测带防抖（点击 2 次、滑动 8 次无触点判定松开）
- 背光控制: LEDC PWM，低电平亮（duty=0 最亮，duty=255 最暗），由 `backlight_manager` 模块统一管理，支持 0-255 亮度调节、渐变动效；自动休眠由主循环统一管理（backlight_manager 的自动休眠已禁用）
- 背光文档: `docs/Backlight_Manager.md`
- 初始化顺序: NVS → LCD → LVGL → 触摸 → UI → 存储 → Petdex → Buddy 状态/统计 → 动画 → 电池 → 通信任务 → WiFi/TCP → 背光
- WiFi 开关受 `BuddySettings.wifi`（NVS `s_wifi`）控制；TCP Server 始终监听 8080 端口
- 弹窗交互: 主菜单/设置/审批弹窗的触摸点击动作统一由 `main.c` 的 `execute_menu_action` / `execute_settings_action` / `execute_approval_action` 处理（BOOT 键确认 + 触摸点击共享同一逻辑）
