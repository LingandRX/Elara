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

#ifdef __cplusplus
}
#endif

#endif /* LVGL_TOUCH_H */
