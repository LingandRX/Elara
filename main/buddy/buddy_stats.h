/**
 * @file buddy_stats.h
 * Buddy 统计系统 - NVS 持久化
 */

#ifndef BUDDY_STATS_H
#define BUDDY_STATS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOKENS_PER_LEVEL 50000

typedef struct {
    uint32_t nap_seconds;
    uint16_t approvals;
    uint16_t denials;
    uint16_t velocity[8];   /* 响应秒数环形缓冲区 */
    uint8_t  vel_idx;
    uint8_t  vel_count;
    uint8_t  level;
    uint32_t tokens;
} BuddyStats;

/* 持久化设置 */
typedef struct {
    bool sound;
    bool wifi;
    bool led;
    bool hud;
    bool auto_sleep;    /* true=启用自动息屏, false=禁用 */
    uint8_t clock_rot;  /* 0=auto 1=portrait 2=landscape */
} BuddySettings;

/* 初始化 */
void buddy_stats_init(void);
void buddy_settings_init(void);

/* 统计操作 */
void buddy_stats_on_approval(uint32_t seconds_to_respond);
void buddy_stats_on_denial(void);
void buddy_stats_on_bridge_tokens(uint32_t bridge_total);
void buddy_stats_on_nap_end(uint32_t seconds);
void buddy_stats_on_wake(void);
bool buddy_stats_poll_level_up(void);

/* 查询 */
BuddyStats*   buddy_stats_get(void);
uint16_t      buddy_stats_median_velocity(void);
uint8_t       buddy_stats_mood_tier(void);
uint8_t       buddy_stats_energy_tier(void);
uint8_t       buddy_stats_fed_progress(void);

/* 设置操作 */
BuddySettings* buddy_settings_get(void);
void           buddy_settings_load(void);
void           buddy_settings_save(void);

/* 宠物名/主人名 */
void        pet_name_set(const char *name);
const char* pet_name_get(void);
void        pet_name_load(void);
void        owner_name_set(const char *name);
const char* owner_name_get(void);
void        owner_name_load(void);

/* 物种索引 */
void     species_idx_save(uint8_t idx);
uint8_t  species_idx_load(void);

/* 重置 */
void buddy_stats_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_STATS_H */
