# Elara 项目智能助手指南

## 项目概述
基于 ESP-IDF 的 ESP32-S3 C 项目

## 硬件引脚

| 功能 | GPIO |
|------|------|
| SPI MOSI | 13 |
| SPI CLK | 10 |
| LCD DC | 11 |
| LCD CS | 12 |
| LCD RST | 9 |
| I2C SDA | 47 |
| I2C SCL | 48 |

## 架构
- **目标芯片**: ESP32-S3 (16MB Flash, 8MB PSRAM 八线模式 @ 80MHz)
- **开发框架**: ESP-IDF v6.0.1
- **入口点**: `main/main.c` → `user_top_init()`

## 组件结构
```
```

## 依赖项
- `espressif/led_strip: ^3.0.1` (通过 ESP-IDF 组件管理器管理)

## 构建命令
```bash
# 设置目标芯片 (仅首次构建时需要)
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录并监控串口输出
idf.py -p COM5 flash monitor

# 仅烧录
idf.py -p COM5 flash

# 菜单配置
idf.py menuconfig

# 清理构建产物
idf.py fullclean
```

## 重要配置
- `sdkconfig.defaults` 定义基础配置 (目标芯片、Flash 大小、PSRAM)
- `sdkconfig` 为生成文件 - 请勿直接编辑
- `sdkconfig.defaults` 纳入 Git 版本控制; `sdkconfig` 已加入 Git 忽略列表
- CPU 频率: 240MHz
- FreeRTOS 节拍率: 1000Hz

## 硬件配置
- **开发板**: ESP32-S3 (内置 USB-JTAG)
- **烧录方式**: JTAG (通过内置 USB)
- **串口号**: COM5 (Windows)
- **OpenOCD 配置**: `board/esp32s3-builtin.cfg`

## 开发注意事项
- RMT 时钟分辨率: 10MHz
- `.clangd` 已配置为 ESP 平台，编译数据库位于 `build/`