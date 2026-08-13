/**
 * @file lvgl_touch.h
 * LVGL 触摸输入驱动适配层
 */

#ifndef LVGL_TOUCH_H
#define LVGL_TOUCH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 LVGL 触摸输入驱动
 */
void lvgl_touch_init(void);

/**
 * 获取 LVGL 输入设备对象
 * @return lv_indev_t 指针
 */
lv_indev_t *lvgl_get_touch_indev(void);

/**
 * 滑动方向回调 (驱动层检测, 不依赖 LVGL 手势系统)
 * @param dir LV_DIR_LEFT/RIGHT/TOP/BOTTOM
 */
typedef void (*lvgl_touch_swipe_cb_t)(lv_dir_t dir);

/**
 * 注册滑动方向回调
 * @param cb 回调, 可为 NULL 取消
 */
void lvgl_touch_set_swipe_cb(lvgl_touch_swipe_cb_t cb);

/**
 * 触摸按下回调 (驱动层检测到新的触点按下时调用)
 */
typedef void (*lvgl_touch_press_cb_t)(void);

/**
 * 注册触摸按下回调（用于唤醒屏幕/重置空闲计时）
 * @param cb 回调, 可为 NULL 取消
 */
void lvgl_touch_set_press_cb(lvgl_touch_press_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_TOUCH_H */
