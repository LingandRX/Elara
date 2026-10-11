# Elara 项目总结

## 项目概述

Elara 是一个基于 ESP32-S3 的桌面宠物 / AI 伴侣固件，使用 **ESP32-S3-Touch-LCD-1.9** 开发板，配备 1.9 英寸触摸屏。主 UI 路径为 Buddy 状态机 + **ASCII 角色动画**（图片/精灵图播放已移除）。

## 技术栈

| 组件 | 技术 | 版本 / 说明 |
|------|------|-------------|
| 主控芯片 | ESP32-S3 | - |
| 开发框架 | ESP-IDF | 推荐 6.0.x；组件约束 `>=5.0`（`main/idf_component.yml`） |
| 图形库 | LVGL | `^8.4.0` |
| 显示驱动 | ST7789V2 | `lv_port_disp` + esp_lcd |
| 触摸芯片 | CST816T | I2C 0x15 |
| 通信 | UART + TCP:8080 | JSON / 明文 wifi 命令 |

## 硬件规格

- **屏幕**: 1.9 英寸 TFT LCD, 170×320 像素, RGB565
- **触摸**: 电容式触摸, I2C 接口, 单点触摸
- **存储**: 16MB Flash, 8MB PSRAM（LittleFS 挂载于 `/spiffs`）
- **通信**: USB-Serial / UART，以及 Wi-Fi TCP 8080

## 软件架构（简图）

```
应用层: Buddy UI / ASCII 动画 / WiFi UI / cmd_proc
中间件: LVGL 8.4 / FreeRTOS / cJSON
驱动层: lv_port_disp(ST7789) / esp_touch(CST816T) / uart_comm / wifi_manager
硬件层: ESP32-S3-Touch-LCD-1.9
```

## 文件结构（与源码一致）

```
Elara/
├── main/
│   ├── main.c
│   ├── display/          # lv_port_disp, backlight_manager, lcd_config
│   ├── input/            # lvgl_touch
│   ├── ui/               # buddy/, pet_*, wifi_ui, 遗留 lvgl_chat_ui
│   ├── buddy/            # buddy_state, buddy_stats
│   ├── battery/
│   ├── comm/             # uart_comm, tcp_server, sntp_sync, xfer
│   ├── wifi_manager.c
│   ├── storage_manager.c
│   └── idf_component.yml
├── components/           # i2c_bsp, esp_touch, lv_conf.h
├── docs/
├── tools/                # elara_host.py, verify_structure.py, ...
├── Makefile
├── CMakeLists.txt
├── sdkconfig.defaults
└── AGENTS.md
```

## 主要功能

### 1. Buddy 主界面
- Persona 状态机、时钟、菜单、设置
- 电池 / Wi-Fi / 背光相关 HUD

### 2. ASCII 角色动画
- 18 种角色 × 7 种状态，由 `buddy_anim` 纯 CPU 渲染字符画
- 图片/精灵图播放（Petdex）与 `{"type":"petdex"}` 命令已移除

### 3. 触摸交互
- CST816T 直接输出屏幕像素坐标
- 当前为轮询读取（未用 TP_INT）

### 4. 通信与网络
- Wi-Fi：NVS `wifinet` 多网络；无配置时为空列表（无出厂硬编码凭据）
- 明文：`wifi <ssid> <password>`
- `upload` 仍直接处理；`petdex` 已随图片播放移除
- 遗留 `status` / `chat` / `progress`：**不再入命令队列**；Buddy JSON 另路径处理

## 开发指南

```bash
. $IDF_PATH/export.sh
make set-target    # 或 idf.py set-target esp32s3
make build
make flash-monitor PORT=/dev/ttyACM0
```

依赖与版本以 `main/idf_component.yml` 为准。

## 参考资料

- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL 文档](https://docs.lvgl.io/)
- [开发板文档](ESP32-S3-Touch-LCD-1.9.md)
