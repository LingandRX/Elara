# Changelog

本文件记录 Elara 项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Added
- Wi-Fi configuration support via UART terminal (`wifi <ssid> <password>` command)
- NVS storage integration for automatic Wi-Fi credential saving and loading across reboots
- Dedicated Wi-Fi Settings UI page to display instructions, current SSID, and IPv4 address
- Mapped BOOT button to toggle the Wi-Fi Settings UI page
- Wi-Fi connection status icon added to the main LVGL status bar
- LVGL v9.1 图形库集成
- LVGL 显示驱动适配层 (`main/display/lv_port_disp.c`)
- LVGL 触摸输入适配层 (`main/input/lvgl_touch.c`)
- LVGL 版本聊天界面 (`main/ui/lvgl_chat_ui.c`)
- LVGL 自定义控件库 (`main/ui/lvgl_widgets.c`)
- 官方 esp_lcd_sh8601 组件 (`components/esp_lcd_sh8601/`)
- LCD 配置常量 (`main/display/lcd_config.h`)
- 构建脚本 (`build.sh`)
- 代码格式化配置 (`.clang-format`, `.editorconfig`)
- 项目文档
  - 开发板文档 (`docs/ESP32-S3-Touch-LCD-1.9.md`)
  - LVGL 集成指南 (`docs/LVGL_Integration.md`)
  - 项目总结 (`docs/Project_Summary.md`)
- 示例代码 (`examples/lvgl_example.c`)
- 字体生成工具 (`tools/generate_font.py`)

### Changed
- 重构 LCD 驱动：从自定义 `sh8601.c` 迁移到官方 `esp_lcd_sh8601` 组件
- 更新 `main.c` 使用 ESP-IDF 标准 LCD API
- 更新 `lv_port_disp.c` 使用 `esp_lcd_panel_draw_bitmap()`
- 背光控制移至 `lv_port_disp.c`，使用 LEDC PWM
- 更新 `main/CMakeLists.txt` 添加 `esp_lcd_sh8601` 依赖

### Removed
- 自定义 SH8601 LCD 驱动 (`main/display/sh8601.c`, `main/display/sh8601.h`)

### Fixed
- 修复字节序双重交换导致的颜色错误（移除多余的 `__builtin_bswap16()` 调用）
- 修复 I2C 驱动未初始化导致的触摸崩溃

## [0.1.0] - 2026-05-30

### Added
- 初始项目结构
- SH8601 LCD 驱动
- 自定义聊天界面
- UART 通信协议
- I2C 驱动封装
- CST816T 触摸驱动
- 中文字体支持 (HZK 格式)

## 版本说明

- **Unreleased**: 开发中的版本
- **0.1.0**: 初始版本

## 变更类型

- **Added**: 新功能
- **Changed**: 已有功能的变更
- **Deprecated**: 即将移除的功能
- **Removed**: 已移除的功能
- **Fixed**: Bug 修复
- **Security**: 安全相关变更
