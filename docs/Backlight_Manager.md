# 背光管理模块使用指南

## 概述

背光管理模块 (`main/display/backlight_manager.c`) 是一个独立的背光控制组件，基于 ESP32 LEDC PWM 驱动实现。它提供亮度无级调节、自动休眠/唤醒、渐变动效等功能，与显示驱动解耦，便于独立维护和扩展。

## 硬件特性

- **控制引脚**: GPIO14 (`LCD_PIN_BL`)
- **驱动方式**: LEDC PWM，8-bit 分辨率，50kHz 频率
- **极性**: 低电平亮（反相控制）
  - `duty = 0`   → 最亮
  - `duty = 255` → 关闭

## 配置文件

所有默认配置位于 `main/display/lcd_config.h`：

| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `BL_DEFAULT_BRIGHTNESS` | `255` | 开机默认亮度（255=最亮） |
| `BL_SLEEP_TIMEOUT_MS` | `30000` | 无操作自动休眠超时（毫秒） |
| `BL_DIM_BRIGHTNESS` | `0` | 休眠时亮度（0=直接关闭） |
| `BL_FADE_STEP_MS` | `20` | 渐变动效每步间隔（毫秒） |
| `BL_FADE_STEP_DELTA` | `8` | 渐变动效每步亮度变化量 |

修改这些宏后重新编译即可生效。

## API 参考

### 初始化

```c
#include "display/backlight_manager.h"

// 需在 lv_port_disp_init() 之后调用
backlight_manager_init(BL_DEFAULT_BRIGHTNESS);
backlight_set_on(true);
```

### 亮度控制

```c
// 设置具体亮度 (0-255)
backlight_set_brightness(128);   // 约 50% 亮度
backlight_set_brightness(255);   // 最亮
backlight_set_brightness(0);     // 关闭

// 开关背光（兼容旧接口）
backlight_set_on(true);   // 最亮
backlight_set_on(false);  // 关闭

// 获取当前亮度
uint8_t level = backlight_get_brightness();
```

### 自动休眠

```c
// 启用/禁用自动休眠
// 参数: 是否启用, 超时时间(ms), 休眠时亮度(0=关闭)
backlight_enable_auto_sleep(true, 60000, 0);   // 60秒无操作关闭
backlight_enable_auto_sleep(true, 30000, 20);  // 30秒无操作降微亮
backlight_enable_auto_sleep(false, 0, 0);      // 禁用自动休眠
```

**休眠机制**：
- 内部任务每秒检测 `lv_disp_get_inactive_time()`（LVGL 无操作时间）
- 超过超时时间后自动降低亮度到 `dim_brightness`
- 检测到用户触摸/交互时自动恢复之前的亮度
- 状态变化通过 `backlight_is_sleeping()` 可查询

### 渐变动效

```c
// 启用/禁用渐变动效
// 参数: 是否启用, 每步间隔(ms), 每步亮度变化量
backlight_set_fade_enabled(true, 15, 10);   // 快速平滑
backlight_set_fade_enabled(true, 30, 4);    // 缓慢柔和
backlight_set_fade_enabled(false, 0, 0);    // 瞬间切换
```

**效果说明**：
- 启用后，`backlight_set_brightness()` 会平滑过渡而非瞬间切换
- 内部使用 FreeRTOS 软件定时器逐次调整 duty
- 渐变过程中新的亮度指令会覆盖目标值

### 状态查询

```c
// 是否处于自动休眠状态
bool sleeping = backlight_is_sleeping();

// 渐变动效是否启用
bool fading = backlight_is_fade_enabled();
```

## 完整使用示例

```c
#include "display/backlight_manager.h"
#include "display/lcd_config.h"

void app_main(void) {
    // ... LCD 和 LVGL 初始化 ...
    lv_port_disp_init(panel);
    lvgl_touch_init();

    // 初始化背光管理器（默认最亮）
    backlight_manager_init(BL_DEFAULT_BRIGHTNESS);
    backlight_set_on(true);

    // 配置 60 秒自动休眠，休眠时关闭
    backlight_enable_auto_sleep(true, 60000, 0);

    // 启用渐变动效
    backlight_set_fade_enabled(true, BL_FADE_STEP_MS, BL_FADE_STEP_DELTA);

    // 之后在其他模块中可调节亮度
    // backlight_set_brightness(180);  // 调到约 70% 亮度
}

// 在其他任务中根据电量调节亮度
void on_battery_low(void) {
    backlight_set_brightness(80);  // 低电量降低亮度省电
}
```

## 注意事项

1. **初始化顺序**: `backlight_manager_init()` 必须在 `lv_port_disp_init()` 之后调用，因为后者负责 LEDC PWM 硬件初始化
2. **线程安全**: 所有 API 都是线程安全的，内部使用互斥锁保护
3. **休眠与手动设置**: 用户在休眠期间手动设置亮度后，下次触摸唤醒仍会恢复到休眠前保存的亮度
4. **定时器依赖**: 渐变动效依赖 FreeRTOS 软件定时器，需确保 `CONFIG_FREERTOS_USE_TIMERS` 已启用（ESP-IDF 默认启用）
