# Changelog

本文件记录 Elara 项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
并且本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Added
- LVGL v9.1 图形库集成
- LVGL 显示驱动适配层 (`main/display/lvgl_display.c`)
- LVGL 触摸输入适配层 (`main/input/lvgl_touch.c`)
- LVGL 版本聊天界面 (`main/ui/lvgl_chat_ui.c`)
- LVGL 自定义控件库 (`main/ui/lvgl_widgets.c`)
- LVGL 中文字体支持 (`main/font/lv_font_chinese_16.c`)
- 构建脚本 (`build.sh`)
- 代码格式化配置 (`.clang-format`, `.editorconfig`)
- 项目文档
  - 开发板文档 (`docs/ESP32-S3-Touch-LCD-1.9.md`)
  - LVGL 集成指南 (`docs/LVGL_Integration.md`)
  - 项目总结 (`docs/Project_Summary.md`)
- 示例代码 (`examples/lvgl_example.c`)
- 字体生成工具 (`tools/generate_font.py`)

### Changed
- 更新 `main.c` 使用 LVGL UI 替代旧版 UI
- 更新 `main/CMakeLists.txt` 包含新的源文件
- 更新 `AGENTS.md` 添加 LVGL 相关说明

### Fixed
- 无

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
