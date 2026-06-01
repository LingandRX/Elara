/**
 * @file backlight_manager.h
 * 背光管理器 - 独立模块，管理亮度调节、自动休眠、渐变动效
 */

#ifndef BACKLIGHT_MANAGER_H
#define BACKLIGHT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化背光管理器
 * 注意：LEDC PWM 需已由 lv_port_disp_init() 提前初始化
 * @param default_brightness 默认亮度 (0-255, 255=最亮)
 */
void backlight_manager_init(uint8_t default_brightness);

/**
 * 设置背光亮度
 * @param level 0-255, 0=关闭/最暗, 255=最亮
 */
void backlight_set_brightness(uint8_t level);

/**
 * 开关背光（兼容旧接口）
 * @param on true=最亮, false=关闭
 */
void backlight_set_on(bool on);

/**
 * 获取当前亮度
 * @return 当前亮度 0-255
 */
uint8_t backlight_get_brightness(void);

/**
 * 配置自动休眠
 * @param enable 是否启用
 * @param timeout_ms 无操作超时时间（毫秒），建议 >= 5000
 * @param dim_brightness 休眠时降低到的亮度（0-255），0 表示直接关闭
 */
void backlight_enable_auto_sleep(bool enable, uint32_t timeout_ms, uint8_t dim_brightness);

/**
 * 启用/禁用渐变动效
 * @param enable 是否启用
 * @param step_ms 每步间隔（毫秒）
 * @param step_delta 每步亮度变化量
 */
void backlight_set_fade_enabled(bool enable, uint32_t step_ms, uint8_t step_delta);

/**
 * 获取当前是否处于休眠状态
 * @return true=休眠中, false=正常
 */
bool backlight_is_sleeping(void);

/**
 * 获取渐变动效是否启用
 * @return true=启用, false=禁用
 */
bool backlight_is_fade_enabled(void);

/**
 * 获取当前自动休眠超时时间
 * @return 超时时间（毫秒）
 */
uint32_t backlight_get_sleep_timeout_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_MANAGER_H */
