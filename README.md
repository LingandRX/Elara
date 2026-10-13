# Elara

基于 ESP32-S3 的桌面宠物交互终端，使用 LVGL 图形库实现 Buddy 状态机 UI 与 **ASCII 角色动画**（图片播放已移除）。

## 特性

- 🎨 **LVGL 图形界面**: 使用 LVGL ^8.4.0 实现 Buddy 主界面与动画
- 🐾 **ASCII 角色动画**: 18 种角色 × 7 种状态，纯 CPU 字符画渲染（无 GPU 依赖）
- 👆 **触摸交互**: 支持电容式触摸屏手势与控制（CST816T）
- 📡 **双模通信**: 支持 USB-Serial 串口与 WiFi/TCP 网络通信（端口 8080）
- 🚀 **文件上传**: 上位机工具支持向设备 LittleFS 传输文件

## 硬件

- **开发板**: ESP32-S3-Touch-LCD-1.9
- **屏幕**: 1.9 英寸 TFT LCD (170×320, ST7789V2)
- **触摸**: 电容式触摸屏 (CST816T)
- **存储**: 16MB Flash, 8MB PSRAM（分区表含 factory + storage/LittleFS）

## 上位机工具

项目提供 Python 客户端脚本 `tools/elara_host.py`。

> **说明**：设备已移除图片/精灵图播放（Petdex），主页统一使用 **ASCII 角色动画**。
> `petdex` 与 `upload-sheet` 命令已下线；`upload` 仍可用于传输任意文件。

### 常用上位机控制命令

```bash
# 设置 Wi-Fi（会写入 NVS；无出厂硬编码凭据）
python tools/elara_host.py --host <设备IP> wifi --ssid "MyWiFi" --password "12345678"

# 上传文件到设备
python tools/elara_host.py --host <设备IP> upload --file firmware.bin --path firmware.bin

# 交互式控制台模式
python tools/elara_host.py --host <设备IP>
```

> 说明：旧版 `{"type":"chat"}` / `{"type":"status"}` 命令不再入设备命令队列；设备侧主路径为会话 JSON（session/time 等）。详见 [上位机通信协议](docs/上位机通信协议.md)。

## 快速开始

### 环境准备

1. 安装 ESP-IDF（推荐 6.0.x；组件约束见 `main/idf_component.yml`：`idf >= 5.0`）
2. 激活环境后进入本仓库根目录

```bash
. $IDF_PATH/export.sh   # 或你本机的 IDF 激活脚本
```

### 编译烧录

以 Makefile / `idf.py` 为准：

```bash
# 设置目标芯片
make set-target
# 或: idf.py set-target esp32s3

# 编译
make build
# 或: idf.py build

# 烧录并监控（按实际串口修改 PORT）
make flash-monitor PORT=/dev/ttyACM0
# 或: idf.py -p /dev/ttyACM0 flash monitor
```

## 项目结构

```
Elara/
├── main/                    # 主程序
│   ├── display/            # 显示端口 (lv_port_disp / ST7789 / 背光)
│   ├── input/              # 触摸输入 (CST816T → LVGL)
│   ├── ui/                 # Buddy（ASCII 动画）/ WiFi UI
│   ├── buddy/              # Buddy 状态机与统计
│   ├── battery/            # 电池监测
│   ├── comm/               # UART / TCP / SNTP / 上传
│   ├── wifi_manager.c      # WiFi 多网络管理
│   ├── storage_manager.c   # LittleFS 挂载
│   └── idf_component.yml   # 组件依赖（真相源）
├── components/             # i2c_bsp、esp_touch、lv_conf.h
├── docs/                   # 详细文档
├── tools/                  # 上位机与校验脚本
├── Makefile                # Make 封装 idf.py
└── AGENTS.md               # 工程指南
```

## 文档

- [上位机通信协议](docs/上位机通信协议.md) - JSON 协议、上传规范及事件说明
- [WiFi 管理器](docs/WiFi_Manager.md) - Wi-Fi 连接管理与状态说明
- [开发板文档](docs/ESP32-S3-Touch-LCD-1.9.md) - 硬件规格和引脚定义
- [LVGL 集成指南](docs/LVGL_Integration.md) - LVGL 使用说明
- [项目总结](docs/Project_Summary.md) - 架构设计和开发指南

## 开发

### 代码风格

```bash
make format
make lint
```

## 许可证

本项目采用 MIT 许可证。
