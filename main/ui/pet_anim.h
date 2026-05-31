/**
 * @file pet_anim.h
 * Pet 动画状态与配置
 */

#ifndef PET_ANIM_H
#define PET_ANIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Pet 动作状态枚举
 * 对应 Sprite 图中的行号
 */
typedef enum {
    PET_ANIM_IDLE = 0,      /* 空闲 (Row 0) */
    PET_ANIM_RUN_RIGHT,     /* 向右跑 (Row 1) */
    PET_ANIM_RUN_LEFT,      /* 向左跑 (Row 2) */
    PET_ANIM_WAVING,        /* 挥手 (Row 3) */
    PET_ANIM_JUMPING,       /* 跳跃 (Row 4) */
    PET_ANIM_FAILED,        /* 失败 (Row 5) */
    PET_ANIM_WAITING,       /* 等待 (Row 6) */
    PET_ANIM_RUNNING,       /* 跑步 (Row 7) */
    PET_ANIM_REVIEW,        /* 评审 (Row 8) */
    PET_ANIM_DEADLOOP,      /* 死循环爆炸告警 */
    PET_ANIM_MAX
} PetAnimState;

/**
 * Pet 动画配置结构体
 */
typedef struct {
    PetAnimState state;     /* 状态 */
    const char *name;       /* 状态名称 */
    const char *dir_name;   /* 目录名称 (用于序列帧小图模式) */
    uint8_t row;            /* Sprite 行号 */
    uint8_t frames;         /* 动画帧数 */
    uint16_t interval_ms;   /* 每帧间隔时间 (ms) */
} PetAnimConfig;

/**
 * 获取指定状态的动画配置
 * @param state 动作状态
 * @return 配置结构体指针，若未找到返回 NULL
 */
const PetAnimConfig* pet_anim_get_config(PetAnimState state);

/**
 * 根据名称获取动作状态
 * @param name 状态名称 (不区分大小写)
 * @return PetAnimState
 */
PetAnimState pet_anim_get_state_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* PET_ANIM_H */
