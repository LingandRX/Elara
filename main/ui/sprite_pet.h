/**
 * @file sprite_pet.h
 * 精灵图宠物组件 - 可在任意父容器中显示并播放动画帧
 * 复用 Petdex 精灵图资源 (/spiffs/sprite.png 精灵图 或 /spiffs/sprites/<dir>/ 序列帧)
 */

#ifndef SPRITE_PET_H
#define SPRITE_PET_H

#include "lvgl.h"
#include "pet_anim.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SpritePet SpritePet;

/**
 * 创建精灵图宠物组件
 * @param parent 父容器
 * @param w      视口宽度 (px)
 * @param h      视口高度 (px)
 * @return SpritePet 句柄，失败返回 NULL
 */
SpritePet *sprite_pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/**
 * 删除精灵图宠物组件
 */
void sprite_pet_delete(SpritePet *sp);

/**
 * 设置动画状态
 * @param sp    组件句柄
 * @param state 动画状态
 */
void sprite_pet_set_state(SpritePet *sp, PetAnimState state);

/**
 * 推进动画（由外部渲染循环调用，按帧间隔自动换帧）
 * @param sp 组件句柄
 */
void sprite_pet_update(SpritePet *sp);

/**
 * 显示/隐藏组件
 * @param sp      组件句柄
 * @param visible 是否可见
 */
void sprite_pet_set_visible(SpritePet *sp, bool visible);

/**
 * 查询组件当前是否可见
 */
bool sprite_pet_is_visible(const SpritePet *sp);

/**
 * 查询组件是否有可用精灵图资源（无资源时无法显示）
 */
bool sprite_pet_ready(const SpritePet *sp);

/**
 * 重新加载精灵图资源（存储挂载后调用，或需要重新检测资源时调用）
 */
void sprite_pet_reload(SpritePet *sp);

#ifdef __cplusplus
}
#endif

#endif /* SPRITE_PET_H */
