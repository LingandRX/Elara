/**
 * @file pet_anim.c
 * Pet 动画配置实现
 */

#include "pet_anim.h"
#include <string.h>
#include <strings.h>

/* 动画配置表 (Petdex 标准 9 行映射 + 告警状态) */
static const PetAnimConfig s_pet_anim_configs[PET_ANIM_MAX] = {
    {PET_ANIM_IDLE,      "Idle",      0, 6, 150}, /* Row 0: 待机 */
    {PET_ANIM_RUN_RIGHT, "Run Right", 1, 8, 100}, /* Row 1: 向右跑 */
    {PET_ANIM_RUN_LEFT,  "Run Left",  2, 8, 100}, /* Row 2: 向左跑 */
    {PET_ANIM_WAVING,    "Waving",    3, 4, 200}, /* Row 3: 挥手/打招呼 */
    {PET_ANIM_JUMPING,   "Jumping",   4, 5, 120}, /* Row 4: 跳跃 */
    {PET_ANIM_FAILED,    "Failed",    5, 8, 150}, /* Row 5: 失败/受伤 */
    {PET_ANIM_WAITING,   "Waiting",   6, 6, 180}, /* Row 6: 等待/休息 */
    {PET_ANIM_RUNNING,   "Action",    7, 6, 100}, /* Row 7: 执行/动作 */
    {PET_ANIM_REVIEW,    "Inspect",   8, 6, 150}, /* Row 8: 思考/检查 */
    {PET_ANIM_DEADLOOP,  "Deadloop",  9, 12, 90}  /* Row 9: 死循环告警 */
};

const PetAnimConfig* pet_anim_get_config(PetAnimState state) {
    if (state >= PET_ANIM_MAX) return NULL;
    return &s_pet_anim_configs[state];
}

PetAnimState pet_anim_get_state_by_name(const char *name) {
    if (!name) return PET_ANIM_IDLE;
    
    for (int i = 0; i < PET_ANIM_MAX; i++) {
        if (strcasecmp(name, s_pet_anim_configs[i].name) == 0) {
            return (PetAnimState)i;
        }
    }

    /* 兼容特殊名称映射到 Deadloop */
    if (strcasecmp(name, "dead_loop") == 0 || strcasecmp(name, "dead loop") == 0 ||
        strcasecmp(name, "while1") == 0 || strcasecmp(name, "while(1)") == 0) {
        return PET_ANIM_DEADLOOP;
    }
    
    return PET_ANIM_IDLE;
}
