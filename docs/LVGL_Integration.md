# LVGL 集成指南

本项目已集成 LVGL v9.1 图形库，用于替代原有的自定义 UI 绘制方案。

## 文件结构

```
main/
├── display/
│   ├── sh8601.c          # LCD 驱动
│   ├── sh8601.h
│   ├── lvgl_display.c    # LVGL 显示适配层
│   └── lvgl_display.h
├── input/
│   ├── lvgl_touch.c      # LVGL 触摸适配层
│   └── lvgl_touch.h
├── ui/
│   ├── lvgl_chat_ui.c    # LVGL 版本 UI
│   ├── lvgl_chat_ui.h
│   ├── lvgl_widgets.c    # 自定义控件
│   └── lvgl_widgets.h
```

## 主要 API

### 显示初始化

```c
#include "display/lvgl_display.h"

// 初始化 LVGL 显示驱动
lvgl_display_init(&lcd);
```

### 触摸初始化

```c
#include "input/lvgl_touch.h"

// 初始化 LVGL 触摸驱动
lvgl_touch_init();
```

### UI 操作

```c
#include "ui/lvgl_chat_ui.h"

// 初始化聊天界面
lvgl_chat_ui_init();

// 添加消息
lvgl_chat_ui_add_msg("user", "你好", "happy", false);

// 设置状态
lvgl_chat_ui_set_status("聆听", "listening");

// 清空聊天
lvgl_chat_ui_clear();

// 显示欢迎界面
lvgl_chat_ui_welcome();
```

## LVGL 配置

LVGL 配置文件位于 `components/lvgl/include/lv_conf.h`，主要配置项：

- `LV_COLOR_DEPTH`: 颜色深度 (16 位 RGB565)
- `LV_COLOR_16_SWAP`: RGB565 字节序 (大端)
- `LV_HOR_RES` / `LV_VER_RES`: 屏幕分辨率 (170x320)
- `LV_MEM_SIZE`: LVGL 内存池大小 (48KB)

## 任务结构

系统使用以下 FreeRTOS 任务：

1. **lvgl_tick** (优先级 6): LVGL 时钟源，1ms tick
2. **lvgl_handler** (优先级 5): LVGL 事件处理，200Hz
3. **uart_rx** (优先级 5): UART 接收
4. **cmd_proc** (优先级 5): 命令处理

## 性能优化

- 使用双缓冲模式减少撕裂
- 触摸轮询间隔 10ms (100Hz)
- 显示刷新周期 16ms (~60fps)
- 使用局部刷新减少 SPI 传输量

## 自定义字体

如需添加更多中文字符：

1. 使用 LVGL 字体转换工具: https://lvgl.io/tools/fontconverter
2. 导出为 C 数组格式
3. 替换 LVGL 字体文件

## 常见问题

### 屏幕闪烁
- 检查 LVGL 配置中的 `LV_COLOR_16_SWAP` 是否正确
- 确保使用双缓冲模式

### 触摸不准确
- 调整 `lvgl_touch.c` 中的坐标映射系数
- 检查触摸芯片 I2C 通信是否正常

### 内存不足
- 减小 `LV_MEM_SIZE` 配置
- 减少显示缓冲区大小
- 使用 PSRAM 分配缓冲区
