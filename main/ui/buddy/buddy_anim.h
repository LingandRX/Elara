/**
 * @file buddy_anim.h
 * Buddy ASCII 角色动画系统（18 种角色 × 7 种状态）
 * 从 claude-desktop-buddy 的 buddy.h 迁移
 */

#ifndef BUDDY_ANIM_H
#define BUDDY_ANIM_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 7 种状态函数签名 */
typedef void (*BuddyStateFn)(uint32_t tick, lv_obj_t *canvas, lv_coord_t cx, lv_coord_t cy, lv_color_t body_color);

/* 物种定义 */
typedef struct {
    const char *name;
    lv_color_t body_color;
    BuddyStateFn states[7];  /* sleep, idle, busy, attention, celebrate, dizzy, heart */
} BuddySpecies;

/* 初始化与全局控制 */
void buddy_anim_init(void);
void buddy_anim_tick(uint8_t persona_state, uint32_t global_tick);
void buddy_anim_invalidate(void);

/* 物种切换 */
void buddy_anim_set_species_by_name(const char *name);
void buddy_anim_set_species_idx(uint8_t idx);
void buddy_anim_next_species(void);
void buddy_anim_prev_species(void);
uint8_t buddy_anim_get_species_idx(void);
uint8_t buddy_anim_get_species_count(void);
const char* buddy_anim_get_species_name(void);

/* Peek 模式（半尺寸，用于 info/pet 页面） */
void buddy_anim_set_peek(bool peek);
bool buddy_anim_is_peek(void);

/* 渲染到指定对象 */
void buddy_anim_render_to(lv_obj_t *parent, lv_coord_t cx, lv_coord_t cy);

/* 全局 tick 计数 */
extern uint32_t g_buddy_tick;

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_ANIM_H */
