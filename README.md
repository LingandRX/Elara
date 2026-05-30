# Elara

基于 ESP32-S3 的 AI 聊天设备，使用 LVGL 图形库实现现代化聊天界面。

## 特性

- 🎨 **LVGL 图形界面**: 使用 LVGL v9.1 实现现代化 UI
- 👆 **触摸交互**: 支持电容式触摸屏
- 💬 **聊天功能**: 支持用户/AI/系统消息
- 🎭 **情绪显示**: 根据 AI 情绪变化显示不同状态
- 📡 **UART 通信**: 通过 JSON 协议与上位机通信

## 硬件

- **开发板**: ESP32-S3-Touch-LCD-1.9
- **屏幕**: 1.9 英寸 TFT LCD (170×320, ST7789V2)
- **触摸**: 电容式触摸屏 (CST816T)
- **存储**: 16MB Flash, 8MB PSRAM

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
│   ├── display/            # 显示驱动
│   ├── input/              # 输入驱动
│   ├── ui/                 # 用户界面
│   ├── font/               # 字体资源
│   └── comm/               # 通信协议
├── components/             # 组件库
├── docs/                   # 文档
├── examples/               # 示例代码
├── tools/                  # 工具脚本
├── test/                   # 测试代码
├── build.sh                # 构建脚本
├── Makefile                # Make 命令
└── AGENTS.md               # 项目文档
```

## 文档

- [开发板文档](docs/ESP32-S3-Touch-LCD-1.9.md) - 硬件规格和引脚定义
- [LVGL 集成指南](docs/LVGL_Integration.md) - LVGL 使用说明
- [项目总结](docs/Project_Summary.md) - 架构设计和开发指南
- [变更日志](CHANGELOG.md) - 版本更新记录

## 开发

### 添加新功能

1. **添加新控件**: 编辑 `main/ui/lvgl_widgets.c`
2. **修改界面**: 编辑 `main/ui/lvgl_chat_ui.c`
3. **添加字体**: 使用 LVGL 字体转换工具
4. **修改配置**: 编辑 `components/lvgl/include/lv_conf.h`

### 代码风格

项目使用 clang-format 统一代码风格：

```bash
# 格式化代码
make format

# 检查代码风格
make lint
```

### 运行测试

```bash
# 运行测试
make test
```

## 示例

查看 [examples/lvgl_example.c](examples/lvgl_example.c) 了解 LVGL 使用示例。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

本项目采用 MIT 许可证。

## 致谢

- [LVGL](https://lvgl.io/) - 图形库
- [ESP-IDF](https://idf.espressif.com/) - 开发框架
- [Espressif](https://www.espressif.com/) - 芯片厂商
