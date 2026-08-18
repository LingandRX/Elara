# Elara

基于 ESP32-S3 的 AI 桌面伴侣与宠物交互终端，使用 LVGL 图形库实现现代化聊天与动画界面。

## 特性

- 🎨 **LVGL 图形界面**: 使用 LVGL v9.1 实现现代化 UI，支持薄荷青主题与平滑交互
- 🐾 **Petdex 宠物动画系统**: 内置 10 种状态动作（待机、奔跑、挥手、跳跃、失败、休息、动作、思考、死循环告警等），支持大图与序列帧双模式
- 👆 **触摸交互**: 支持电容式触摸屏手势与控制
- 💬 **智能交互**: 支持流式分片聊天、情绪感知与进度展示
- 📡 **双模通信**: 支持 USB-Serial 串口与 WiFi/TCP 网络通信 (端口 8080)
- 🚀 **精灵图自动切图上传**: 提供上位机工具，支持 WebP/PNG 一键本地切图并上传至设备 SPIFFS 存储

## 硬件

- **开发板**: ESP32-S3-Touch-LCD-1.9
- **屏幕**: 1.9 英寸 TFT LCD (170×320, ST7789V2)
- **触摸**: 电容式触摸屏 (CST816T)
- **存储**: 16MB Flash, 8MB PSRAM (包含 SPIFFS/LittleFS 文件系统分区)

## 上位机工具与精灵图上传

项目提供了功能完备的 Python 客户端脚本 `tools/elara_host.py`。

### 1. 精灵图一键切图与上传 (WiFi/TCP)
直接读取一张包含多动作的整图（如 `spritesheet.webp` 或 `.png`），本地自动切片为 9 组动作共 57 帧 PNG 图片，并通过控制台进度条动态批量上传至设备：

```bash
# 默认上传全部动作序列帧至设备
python tools/elara_host.py --host <设备IP> upload-sheet --file spritesheet.webp

# 自定义精灵名称（前缀路径）
python tools/elara_host.py --host <设备IP> upload-sheet --file spritesheet.webp --name mypet

# 自定义帧分辨率 (默认 96x104)
python tools/elara_host.py --host <设备IP> upload-sheet --file spritesheet.webp --width 96 --height 104
```

### 2. 常用上位机控制命令
```bash
# 切换 Petdex 动画状态
python tools/elara_host.py --host <设备IP> petdex --state "Run Right"

# 发送 AI 聊天消息
python tools/elara_host.py --host <设备IP> chat --role ai --text "你好呀！"

# 设置 Wi-Fi
python tools/elara_host.py --host <设备IP> wifi --ssid "MyWiFi" --password "12345678"

# 交互式控制台模式
python tools/elara_host.py --host <设备IP>
```

## 快速开始

### 环境准备

1. 安装 ESP-IDF v6.0.1
2. 设置环境变量
```bash
. $HOME/esp/esp-idf/export.sh
```

### 编译烧录

```bash
# 设置目标芯片
make set-target

# 编译
make build

# 烧录并监控
make flash-monitor PORT=/dev/tty.usbmodem0
```

### 使用构建脚本

```bash
# 显示帮助
./build.sh help

# 编译项目
./build.sh build

# 烧录并监控
./build.sh flash-monitor
```

## 项目结构

```
Elara/
├── main/                    # 主程序
│   ├── display/            # 显示驱动 (ST7789V2)
│   ├── input/              # 输入驱动 (CST816T)
│   ├── ui/                 # 用户界面 (LVGL 聊天、Petdex 动画页面)
│   ├── font/               # 字体资源
│   ├── storage/            # SPIFFS/LittleFS 存储管理
│   └── comm/               # 通信协议 (UART / TCP Server / 命令解析)
├── components/             # 组件库 (LVGL 等)
├── docs/                   # 详细文档
├── tools/                  # 工具脚本 (上位机、切图工具等)
├── build.sh                # 构建脚本
├── Makefile                # Make 命令
└── AGENTS.md               # 项目架构与指南
```

## 文档

- [上位机通信协议](docs/上位机通信协议.md) - JSON 协议、上传规范及事件说明
- [WiFi 管理器](docs/WiFi_Manager.md) - Wi-Fi 连接管理与状态说明
- [开发板文档](docs/ESP32-S3-Touch-LCD-1.9.md) - 硬件规格和引脚定义
- [LVGL 集成指南](docs/LVGL_Integration.md) - LVGL 使用说明
- [项目总结](docs/Project_Summary.md) - 架构设计和开发指南
- [变更日志](CHANGELOG.md) - 版本更新记录

## 开发

### 代码风格

项目使用 clang-format 统一代码风格：

```bash
# 格式化代码
make format

# 检查代码风格
make lint
```

## 许可证

本项目采用 MIT 许可证。
