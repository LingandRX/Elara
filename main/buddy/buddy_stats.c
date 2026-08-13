/**
 * @file buddy_stats.c
 * Buddy 统计与设置 - NVS 持久化实现
 */

#include "buddy_stats.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BUDDY_STATS";

#define NVS_NAMESPACE "buddy"

static BuddyStats   s_stats = {0};
static BuddySettings s_settings = { true, true, true, true, false, 0, 0 };
static nvs_handle_t s_nvs_handle = 0;
static bool s_nvs_open = false;

/* Token 同步状态 */
static uint32_t s_last_bridge_tokens = 0;
static bool s_tokens_synced = false;
static bool s_level_up_pending = false;

/* 能量状态 */
static uint32_t s_last_nap_end_ms = 0;
static uint8_t  s_energy_at_nap = 3;

/* 脏标记 */
static bool s_dirty = false;

static void _nvs_open(void) {
    if (s_nvs_open) return;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }
    s_nvs_open = true;
}

static void _nvs_close(void) {
    if (!s_nvs_open) return;
    nvs_close(s_nvs_handle);
    s_nvs_open = false;
}

void buddy_stats_init(void) {
    _nvs_open();
    if (!s_nvs_open) return;

    s_stats.nap_seconds = 0;
    s_stats.approvals = 0;
    s_stats.denials = 0;
    s_stats.vel_idx = 0;
    s_stats.vel_count = 0;
    s_stats.level = 0;
    s_stats.tokens = 0;
    memset(s_stats.velocity, 0, sizeof(s_stats.velocity));

    /* 读取 NVS */
    esp_err_t err;
    err = nvs_get_u32(s_nvs_handle, "nap", &s_stats.nap_seconds);
    if (err == ESP_ERR_NVS_NOT_FOUND) s_stats.nap_seconds = 0;

    err = nvs_get_u16(s_nvs_handle, "appr", &s_stats.approvals);
    if (err == ESP_ERR_NVS_NOT_FOUND) s_stats.approvals = 0;

    err = nvs_get_u16(s_nvs_handle, "deny", &s_stats.denials);
    if (err == ESP_ERR_NVS_NOT_FOUND) s_stats.denials = 0;

    uint8_t vidx = 0, vcnt = 0, lvl = 0;
    err = nvs_get_u8(s_nvs_handle, "vidx", &vidx);
    if (err == ESP_ERR_NVS_NOT_FOUND) vidx = 0;

    err = nvs_get_u8(s_nvs_handle, "vcnt", &vcnt);
    if (err == ESP_ERR_NVS_NOT_FOUND) vcnt = 0;

    err = nvs_get_u8(s_nvs_handle, "lvl", &lvl);
    if (err == ESP_ERR_NVS_NOT_FOUND) lvl = 0;

    s_stats.vel_idx = vidx;
    s_stats.vel_count = vcnt;
    s_stats.level = lvl;

    uint32_t tok = 0;
    err = nvs_get_u32(s_nvs_handle, "tok", &tok);
    if (err == ESP_ERR_NVS_NOT_FOUND) tok = 0;
    s_stats.tokens = tok;

    size_t vel_len = sizeof(s_stats.velocity);
    err = nvs_get_blob(s_nvs_handle, "vel", s_stats.velocity, &vel_len);
    if (err == ESP_ERR_NVS_NOT_FOUND || vel_len != sizeof(s_stats.velocity)) {
        memset(s_stats.velocity, 0, sizeof(s_stats.velocity));
    }

    /* 兼容：如果 level 有值但 tokens 为 0，反推 tokens */
    if (s_stats.tokens == 0 && s_stats.level > 0) {
        s_stats.tokens = (uint32_t)s_stats.level * TOKENS_PER_LEVEL;
    }

    s_dirty = false;
    _nvs_close();
    ESP_LOGI(TAG, "Stats loaded: level=%u, tokens=%lu", s_stats.level, s_stats.tokens);
}

void buddy_settings_init(void) {
    _nvs_open();
    if (!s_nvs_open) return;

    esp_err_t err;
    uint8_t val8;

    err = nvs_get_u8(s_nvs_handle, "s_snd", &val8);
    s_settings.sound = (err == ESP_OK) ? val8 : true;

    err = nvs_get_u8(s_nvs_handle, "s_wifi", &val8);
    s_settings.wifi = (err == ESP_OK) ? val8 : true;

    err = nvs_get_u8(s_nvs_handle, "s_led", &val8);
    s_settings.led = (err == ESP_OK) ? val8 : true;

    err = nvs_get_u8(s_nvs_handle, "s_hud", &val8);
    s_settings.hud = (err == ESP_OK) ? val8 : true;

    err = nvs_get_u8(s_nvs_handle, "s_aslp", &val8);
    s_settings.auto_sleep = (err == ESP_OK) ? val8 : false;

    err = nvs_get_u8(s_nvs_handle, "s_crot", &val8);
    s_settings.clock_rot = (err == ESP_OK && val8 <= 2) ? val8 : 0;

    err = nvs_get_u8(s_nvs_handle, "s_pmode", &val8);
    s_settings.pet_mode = (err == ESP_OK && val8 <= 1) ? val8 : 0;

    _nvs_close();
    ESP_LOGI(TAG, "Settings loaded");
}

static void stats_save_if_dirty(void) {
    if (!s_dirty) return;
    _nvs_open();
    if (!s_nvs_open) return;

    nvs_set_u32(s_nvs_handle, "nap", s_stats.nap_seconds);
    nvs_set_u16(s_nvs_handle, "appr", s_stats.approvals);
    nvs_set_u16(s_nvs_handle, "deny", s_stats.denials);
    nvs_set_u8(s_nvs_handle, "vidx", s_stats.vel_idx);
    nvs_set_u8(s_nvs_handle, "vcnt", s_stats.vel_count);
    nvs_set_u8(s_nvs_handle, "lvl", s_stats.level);
    nvs_set_u32(s_nvs_handle, "tok", s_stats.tokens);
    nvs_set_blob(s_nvs_handle, "vel", s_stats.velocity, sizeof(s_stats.velocity));
    nvs_commit(s_nvs_handle);

    s_dirty = false;
    _nvs_close();
}

void buddy_stats_on_approval(uint32_t seconds_to_respond) {
    s_stats.approvals++;
    s_stats.velocity[s_stats.vel_idx] = (seconds_to_respond > 65535u) ? 65535u : (uint16_t)seconds_to_respond;
    s_stats.vel_idx = (s_stats.vel_idx + 1) % 8;
    if (s_stats.vel_count < 8) s_stats.vel_count++;
    s_dirty = true;
    stats_save_if_dirty();
}

void buddy_stats_on_denial(void) {
    s_stats.denials++;
    s_dirty = true;
    stats_save_if_dirty();
}

void buddy_stats_on_bridge_tokens(uint32_t bridge_total) {
    if (!s_tokens_synced) {
        s_last_bridge_tokens = bridge_total;
        s_tokens_synced = true;
        return;
    }
    if (bridge_total < s_last_bridge_tokens) {
        s_last_bridge_tokens = bridge_total;
        return;
    }
    uint32_t delta = bridge_total - s_last_bridge_tokens;
    s_last_bridge_tokens = bridge_total;
    if (delta == 0) return;

    uint8_t lvl_before = (uint8_t)(s_stats.tokens / TOKENS_PER_LEVEL);
    s_stats.tokens += delta;
    uint8_t lvl_after = (uint8_t)(s_stats.tokens / TOKENS_PER_LEVEL);

    if (lvl_after > lvl_before) {
        s_stats.level = lvl_after;
        s_level_up_pending = true;
        s_dirty = true;
        stats_save_if_dirty();
    }
}

bool buddy_stats_poll_level_up(void) {
    bool r = s_level_up_pending;
    s_level_up_pending = false;
    return r;
}

void buddy_stats_on_nap_end(uint32_t seconds) {
    s_stats.nap_seconds += seconds;
    s_dirty = true;
    stats_save_if_dirty();
}

void buddy_stats_on_wake(void) {
    s_last_nap_end_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s_energy_at_nap = 5;
}

BuddyStats* buddy_stats_get(void) { return &s_stats; }

uint16_t buddy_stats_median_velocity(void) {
    if (s_stats.vel_count == 0) return 0;
    uint16_t tmp[8];
    memcpy(tmp, s_stats.velocity, sizeof(tmp));
    uint8_t n = s_stats.vel_count;
    /* 插入排序 */
    for (uint8_t i = 1; i < n; i++) {
        uint16_t k = tmp[i];
        int8_t j = i - 1;
        while (j >= 0 && tmp[j] > k) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = k;
    }
    return tmp[n / 2];
}

uint8_t buddy_stats_mood_tier(void) {
    uint16_t vel = buddy_stats_median_velocity();
    int8_t tier;
    if (vel == 0) tier = 2;
    else if (vel < 15) tier = 4;
    else if (vel < 30) tier = 3;
    else if (vel < 60) tier = 2;
    else if (vel < 120) tier = 1;
    else tier = 0;

    uint16_t a = s_stats.approvals, d = s_stats.denials;
    if (a + d >= 3) {
        if (d > a) tier -= 2;
        else if (d * 2 > a) tier -= 1;
    }
    if (tier < 0) tier = 0;
    return (uint8_t)tier;
}

uint8_t buddy_stats_energy_tier(void) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t hours_since = (now_ms - s_last_nap_end_ms) / 3600000;
    int8_t e = (int8_t)s_energy_at_nap - (int8_t)(hours_since / 2);
    if (e < 0) e = 0;
    if (e > 5) e = 5;
    return (uint8_t)e;
}

uint8_t buddy_stats_fed_progress(void) {
    return (uint8_t)((s_stats.tokens % TOKENS_PER_LEVEL) / (TOKENS_PER_LEVEL / 10));
}

BuddySettings* buddy_settings_get(void) { return &s_settings; }

void buddy_settings_load(void) {
    buddy_settings_init();
}

void buddy_settings_save(void) {
    _nvs_open();
    if (!s_nvs_open) return;
    nvs_set_u8(s_nvs_handle, "s_snd", s_settings.sound);
    nvs_set_u8(s_nvs_handle, "s_wifi", s_settings.wifi);
    nvs_set_u8(s_nvs_handle, "s_led", s_settings.led);
    nvs_set_u8(s_nvs_handle, "s_hud", s_settings.hud);
    nvs_set_u8(s_nvs_handle, "s_aslp", s_settings.auto_sleep);
    nvs_set_u8(s_nvs_handle, "s_crot", s_settings.clock_rot);
    nvs_set_u8(s_nvs_handle, "s_pmode", s_settings.pet_mode);
    nvs_commit(s_nvs_handle);
    _nvs_close();
}

/* 宠物名/主人名 */
static char s_pet_name[24] = "Pixel";
static char s_owner_name[24] = "";

void pet_name_set(const char *name) {
    if (!name) return;
    strncpy(s_pet_name, name, sizeof(s_pet_name) - 1);
    s_pet_name[sizeof(s_pet_name) - 1] = '\0';
    _nvs_open();
    if (s_nvs_open) {
        nvs_set_str(s_nvs_handle, "pet_name", s_pet_name);
        nvs_commit(s_nvs_handle);
        _nvs_close();
    }
}

const char* pet_name_get(void) { return s_pet_name; }

void pet_name_load(void) {
    _nvs_open();
    if (!s_nvs_open) return;
    size_t len = sizeof(s_pet_name);
    esp_err_t err = nvs_get_str(s_nvs_handle, "pet_name", s_pet_name, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        strcpy(s_pet_name, "Pixel");
    }
    _nvs_close();
}

void owner_name_set(const char *name) {
    if (!name) return;
    strncpy(s_owner_name, name, sizeof(s_owner_name) - 1);
    s_owner_name[sizeof(s_owner_name) - 1] = '\0';
    _nvs_open();
    if (s_nvs_open) {
        nvs_set_str(s_nvs_handle, "owner_name", s_owner_name);
        nvs_commit(s_nvs_handle);
        _nvs_close();
    }
}

const char* owner_name_get(void) { return s_owner_name; }

void owner_name_load(void) {
    _nvs_open();
    if (!s_nvs_open) return;
    size_t len = sizeof(s_owner_name);
    esp_err_t err = nvs_get_str(s_nvs_handle, "owner_name", s_owner_name, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_owner_name[0] = '\0';
    }
    _nvs_close();
}

void species_idx_save(uint8_t idx) {
    _nvs_open();
    if (s_nvs_open) {
        nvs_set_u8(s_nvs_handle, "species_idx", idx);
        nvs_commit(s_nvs_handle);
        _nvs_close();
    }
}

uint8_t species_idx_load(void) {
    uint8_t idx = 0xFF;
    _nvs_open();
    if (s_nvs_open) {
        esp_err_t err = nvs_get_u8(s_nvs_handle, "species_idx", &idx);
        if (err == ESP_ERR_NVS_NOT_FOUND) idx = 0xFF;
        _nvs_close();
    }
    return idx;
}

void buddy_stats_factory_reset(void) {
    _nvs_open();
    if (s_nvs_open) {
        nvs_erase_all(s_nvs_handle);
        nvs_commit(s_nvs_handle);
        _nvs_close();
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_settings = (BuddySettings){ true, true, true, true, false, 0, 0 };
    s_pet_name[0] = '\0';
    s_owner_name[0] = '\0';
    strcpy(s_pet_name, "Pixel");
}
