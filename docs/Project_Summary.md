# Elara 项目总结

## 项目概述

Elara 是一个基于 ESP32-S3 的聊天设备项目，使用 ESP32-S3-Touch-LCD-1.9 开发板，配备 1.9 英寸触摸屏。

## 技术栈

| 组件 | 技术 | 版本 |
|------|------|------|
| 主控芯片 | ESP32-S3 | - |
| 开发框架 | ESP-IDF | v6.0.1 |
| 图形库 | LVGL | v9.1 |
| 显示驱动 | ST7789V2 | - |
| 触摸芯片 | CST816T | - |
| 通信协议 | UART (JSON) | - |

## 硬件规格

- **屏幕**: 1.9 英寸 TFT LCD, 170×320 像素, RGB565
- **触摸**: 电容式触摸, I2C 接口, 单点触摸
- **存储**: 16MB Flash, 8MB PSRAM
- **通信**: UART (115200 baud)

## 软件架构

```
┌─────────────────────────────────────────────────────────┐
│                      应用层 (App)                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  聊天界面     │  │  命令处理     │  │  状态管理     │  │
│  │ lvgl_chat_ui │  │ cmd_process  │  │   status     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│                      中间件层 (Middleware)                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │    LVGL      │  │    FreeRTOS  │  │    JSON      │  │
│  │   v9.1       │  │   Tasks      │  │   Parser     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│                      驱动层 (Driver)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  LCD 驱动    │  │  触摸驱动     │  │  UART 驱动   │  │
│  │   sh8601     │  │   esp_touch  │  │  uart_comm   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│                      硬件层 (Hardware)                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   ST7789V2   │  │   CST816T    │  │   ESP32-S3   │  │
│  │   (SPI)      │  │   (I2C)      │  │   (UART)     │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 文件结构

```
Elara/
├── main/                           # 主程序
│   ├── main.c                     # 入口点
│   ├── display/                   # 显示相关
│   │   ├── sh8601.c/h            # LCD 驱动
│   │   └── lvgl_display.c/h      # LVGL 显示适配
│   ├── input/                     # 输入相关
│   │   └── lvgl_touch.c/h        # 触摸输入适配
│   ├── ui/                        # 用户界面
│   │   ├── lvgl_chat_ui.c/h      # LVGL 版本 UI
│   │   └── lvgl_widgets.c/h      # 自定义控件
│   └── comm/                      # 通信协议
│       └── uart_comm.c/h         # UART 通信
├── components/                    # 组件库
│   ├── lvgl/                     # LVGL 图形库
│   ├── i2c_bsp/                  # I2C 驱动
│   └── esp_touch/                # 触摸控制器驱动
├── docs/                          # 文档
│   ├── ESP32-S3-Touch-LCD-1.9.md # 开发板文档
│   ├── LVGL_Integration.md       # LVGL 集成指南
│   └── Project_Summary.md        # 项目总结
├── tools/                         # 工具脚本
│   └── generate_font.py          # 字体生成脚本
├── examples/                      # 示例代码
│   └── lvgl_example.c            # LVGL 使用示例
├── build.sh                       # 构建脚本
├── CMakeLists.txt                 # 项目配置
├── sdkconfig.defaults             # 默认配置
└── AGENTS.md                      # 项目文档
```

## 主要功能

### 1. 聊天界面
- 消息气泡显示 (用户/AI/系统)
- 状态栏 (空闲/聆听/思考/回复/错误)
- 情绪指示器
- 流式消息支持
- 消息滚动

### 2. 触摸交互
- 单点触摸检测
- 坐标映射 (0-4095 → 0-169/0-319)
- 100Hz 轮询频率

### 3. UART 通信
- JSON 命令协议
- 支持三种命令类型:
  - `status`: 状态更新
  - `chat`: 聊天消息
  - `clear`: 清空聊天

### 4. LVGL 集成
- 双缓冲模式
- 200Hz 事件处理
- 自定义控件库
- 中文字体支持

## 开发指南

### 快速开始

1. 设置 ESP-IDF 环境
```bash
. $HOME/esp/esp-idf/export.sh
```

2. 设置目标芯片
```bash
./build.sh set-target
```

3. 编译项目
```bash
./build.sh build
```

4. 烧录并监控
```bash
./build.sh flash-monitor /dev/tty.usbmodem0
```

### 添加新功能

1. **添加新控件**:
   - 在 `main/ui/lvgl_widgets.c` 中添加控件实现
   - 在 `main/ui/lvgl_widgets.h` 中添加函数声明

2. **修改界面**:
   - 编辑 `main/ui/lvgl_chat_ui.c` 修改聊天界面
   - 使用 LVGL API 创建自定义界面

3. **修改配置**:
   - 编辑 `components/lvgl/include/lv_conf.h` 修改 LVGL 配置
   - 编辑 `sdkconfig.defaults` 修改 ESP-IDF 配置

## 性能优化

### 触摸优化
- 10ms 轮询间隔 (100Hz)
- 坐标映射优化
- 去抖动处理

## 常见问题

## 下一步计划

1. **功能增强**
   - 添加语音识别支持
   - 实现表情动画
   - 添加主题切换功能

2. **性能优化**
   - 优化 LVGL 渲染性能
   - 减少内存占用
   - 提高触摸响应速度

3. **用户体验**
   - 添加手势支持
   - 实现滑动切换
   - 优化动画效果

## 参考资料

- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [LVGL 文档](https://docs.lvgl.io/)
- [ST7789V2 数据手册](https://www.sitronix.com.tw/)
- [CST816T 数据手册](http://www.champion-micro.com/)
