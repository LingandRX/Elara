# LVGL 集成指南

本项目通过 ESP-IDF 组件管理器集成 **LVGL ^8.4.0**（见 `main/idf_component.yml`），主界面为 Buddy UI，显示端口为 `lv_port_disp`（ST7789）。

## 文件结构（与源码一致）

```
main/
├── display/
│   ├── lv_port_disp.c/h      # LVGL 显示端口（esp_lcd + ST7789）
│   ├── lcd_config.h          # 引脚与分辨率
│   └── backlight_manager.c/h # 背光 PWM
├── input/
│   └── lvgl_touch.c/h        # LVGL 触摸适配（CST816T）
├── ui/
│   ├── buddy/                # Buddy 主界面（当前主路径）
│   ├── pet_ui.c / pet_anim.c # Petdex 动画页
│   ├── wifi_ui.c             # WiFi 管理页
│   ├── lvgl_chat_ui.c/h      # 遗留聊天 UI（当前未加入 CMake 链接）
│   └── lvgl_widgets.c/h      # 自定义控件
└── idf_component.yml         # lvgl/lvgl: ^8.4.0
```

全局配置：`components/lv_conf.h`（由组件管理器与工程共同约束）。

## 主要 API

### 显示初始化

```c
#include "display/lv_port_disp.h"

lv_port_disp_init();
```

### 触摸初始化

```c
#include "input/lvgl_touch.h"

lvgl_touch_init();
```

### Buddy / Petdex

主路径为 Buddy 状态机 UI（`buddy_ui_*`）与 Petdex（`pet_ui_*`），而非遗留 `lvgl_chat_ui_*`。

## LVGL 配置要点

- 颜色深度: 16 位 RGB565
- 分辨率: 170×320（竖屏）
- 显示缓冲: 双缓冲，行缓冲见 `LVGL_BUF_LINES`（当前 160）
- Tick: esp_timer 周期驱动（见 `AGENTS.md`）

## 任务结构

1. **lvgl_handler**: LVGL 事件/渲染（动态调度）
2. **uart_rx**: UART 接收
3. **cmd_proc**: 命令队列（当前仅 PETDEX）
4. 主循环内联在 `app_main`（Buddy 状态机 + BOOT 键）

## 常见问题

### 屏幕颜色异常
- 检查 `rgb_ele_order`、`data_endian`、`invert_color`（见 `AGENTS.md`）

### 触摸不准确
- CST816T 直接输出屏幕像素坐标；检查 I2C（SDA 47 / SCL 48，地址 0x15）

### 内存不足
- 调整缓冲行数；大图放 LittleFS / PSRAM
