/**
 * @file pet_ui.h
 * Petdex 动画页面
 */

#ifndef PET_UI_H
#define PET_UI_H

#include "lvgl.h"
#include "pet_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 Petdex 页面
 */
void pet_ui_init(void);

/**
 * 显示/隐藏 Petdex 页面
 * @param show 是否显示
 */
void pet_ui_show(bool show);

/**
 * 查询 Petdex 页面当前是否可见
 * @return true=可见
 */
bool pet_ui_is_visible(void);

/**
 * 刷新 Petdex 精灵图缓存
 */
void pet_ui_refresh(void);

/**
 * 设置当前动画状态
 * @param state 动画状态
 */
void pet_ui_set_state(PetAnimState state);

/**
 * 设置动画状态（通过名称）
 * @param name 状态名称
 */
void pet_ui_set_state_by_name(const char *name);

/**
 * 切换到下一个动画状态
 */
void pet_ui_next_state(void);

/**
 * 切换到上一个动画状态
 */
void pet_ui_prev_state(void);

#ifdef __cplusplus
}
#endif

#endif /* PET_UI_H */
