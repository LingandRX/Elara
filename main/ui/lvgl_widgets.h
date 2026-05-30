/**
 * @file lvgl_widgets.h
 * LVGL 自定义控件
 */

#ifndef LVGL_WIDGETS_H
#define LVGL_WIDGETS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 创建消息气泡
 * @param parent 父对象
 * @param role 角色 ("user", "ai", "system")
 * @param text 消息文本
 * @return 气泡对象
 */
lv_obj_t *lvgl_create_message_bubble(lv_obj_t *parent, const char *role, const char *text);

/**
 * 创建状态指示器
 * @param parent 父对象
 * @param state 状态文本
 * @param color 指示器颜色
 * @return 指示器对象
 */
lv_obj_t *lvgl_create_status_indicator(lv_obj_t *parent, const char *state, lv_color_t color);

/**
 * 创建动画标签
 * @param parent 父对象
 * @param text 标签文本
 * @return 标签对象
 */
lv_obj_t *lvgl_create_animated_label(lv_obj_t *parent, const char *text);

/**
 * 创建进度条
 * @param parent 父对象
 * @return 进度条对象
 */
lv_obj_t *lvgl_create_progress_bar(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_WIDGETS_H */
