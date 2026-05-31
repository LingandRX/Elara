/**
 * @file pet_ui.c
 * Petdex 动画页面实现
 */

#include "pet_ui.h"
#include "storage_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lvgl.h"
#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "PET_UI";

static lv_obj_t *pet_page = NULL;
static lv_obj_t *pet_obj = NULL;
static lv_obj_t *pet_img = NULL;
static lv_obj_t *pet_face_label = NULL;
static lv_obj_t *state_label = NULL;
static lv_obj_t *frame_label = NULL;
static lv_obj_t *deadloop_label = NULL;
static lv_timer_t *anim_timer = NULL;

static PetAnimState current_state = PET_ANIM_IDLE;
static uint8_t current_frame = 1;

/* 精灵图配置 (适配 96x104 规格，节省内存) */
#define SPRITE_PATH "S:/spiffs/sprite.png"
#define FRAME_W 96
#define FRAME_H 104

/* 缩放配置：屏幕宽度 170，高度 320 */
#define DISPLAY_W 170
#define DISPLAY_H 184
#define ZOOM_LEVEL 453

#define DEADLOOP_PARTICLE_COUNT 12
static lv_obj_t *deadloop_particles[DEADLOOP_PARTICLE_COUNT];

static void set_deadloop_particles_hidden(bool hidden) {
    for (int i = 0; i < DEADLOOP_PARTICLE_COUNT; i++) {
        if (!deadloop_particles[i]) continue;
        if (hidden) {
            lv_obj_add_flag(deadloop_particles[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(deadloop_particles[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_deadloop_effect(uint8_t frame) {
    static const int8_t x_dirs[DEADLOOP_PARTICLE_COUNT] = {
        -6, -4, -2, 0, 2, 4, 6, 5, 3, -3, -5, 1
    };
    static const int8_t y_dirs[DEADLOOP_PARTICLE_COUNT] = {
        -5, -2, -6, -4, -5, -1, -3, 2, 5, 4, 1, 6
    };
    static const uint32_t colors[DEADLOOP_PARTICLE_COUNT] = {
        0xFF3030, 0xFF8C00, 0xFFE66D, 0x64B4FF,
        0xB464FF, 0xFFFFFF, 0xFF3030, 0xFF8C00,
        0xFFE66D, 0x64B4FF, 0xB464FF, 0xFFFFFF
    };

    bool blast = frame >= 4 && frame <= 9;
    int32_t shake = (frame % 2 == 0) ? 4 : -4;

    if (pet_img && !lv_obj_has_flag(lv_obj_get_parent(pet_img), LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_x(lv_obj_get_parent(pet_img), (frame < 4 ? shake : 0));
    } else {
        lv_obj_clear_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(pet_obj, DISPLAY_W, DISPLAY_H);
        lv_obj_set_x(pet_obj, frame < 4 ? shake : 0);
        lv_obj_set_style_bg_color(pet_obj, frame < 4 ? lv_color_hex(0xFF3030) : lv_color_hex(0x2D2D37), 0);
        lv_obj_set_style_border_color(pet_obj, lv_color_hex(0xFFE66D), 0);
        lv_obj_set_style_opa(pet_obj, blast ? LV_OPA_30 : LV_OPA_COVER, 0);
    }

    if (pet_face_label) {
        if (frame < 4) {
            lv_label_set_text(pet_face_label, "0xDEAD");
        } else if (frame < 10) {
            lv_label_set_text(pet_face_label, "BOOM");
        } else {
            lv_label_set_text(pet_face_label, "WDT");
        }
        lv_obj_set_style_text_color(pet_face_label, lv_color_hex(0xFFFFFF), 0);
    }

    if (deadloop_label) {
        lv_obj_clear_flag(deadloop_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(deadloop_label, frame < 4 ? "deadloop: watchdog starving" : "while(1) detected\nplease feed watchdog");
        lv_obj_set_style_text_color(deadloop_label, frame < 4 ? lv_color_hex(0xFF3030) : lv_color_hex(0xFFE66D), 0);
    }

    set_deadloop_particles_hidden(!blast);
    if (!blast) return;

    int32_t radius = 6 + (frame - 4) * 8;
    for (int i = 0; i < DEADLOOP_PARTICLE_COUNT; i++) {
        lv_obj_t *particle = deadloop_particles[i];
        if (!particle) continue;

        int32_t size = 8 - ((frame - 4) / 2);
        if (size < 3) size = 3;
        lv_obj_set_size(particle, size, size);
        lv_obj_set_style_bg_color(particle, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_opa(particle, frame > 8 ? LV_OPA_50 : LV_OPA_COVER, 0);
        lv_obj_align(particle, LV_ALIGN_CENTER, x_dirs[i] * radius / 2, -20 + y_dirs[i] * radius / 2);
    }
}

static void reset_deadloop_effect(void) {
    if (!pet_obj) return;

    if (pet_img && !lv_obj_has_flag(lv_obj_get_parent(pet_img), LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_x(lv_obj_get_parent(pet_img), 0);
    } else {
        lv_obj_clear_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(pet_obj, DISPLAY_W, DISPLAY_H);
        lv_obj_set_style_opa(pet_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(pet_obj, lv_color_hex(0xFFFFFF), 0);
    }
    set_deadloop_particles_hidden(true);

    if (deadloop_label) {
        lv_obj_add_flag(deadloop_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (pet_face_label) {
        lv_label_set_text(pet_face_label, "Pixel");
        lv_obj_set_style_text_color(pet_face_label, lv_color_hex(0x080810), 0);
    }
}

/* 检查并更新精灵图 */
static void check_sprite_image(void) {
    const char *fs_path = "/spiffs/sprite.png";
    const char *lv_path = "S:/spiffs/sprite.png";
    
    /* 检查是否处于序列帧模式 */
    const PetAnimConfig *cfg = pet_anim_get_config(current_state);
    if (cfg) {
        char seq_fs_path[64];
        char seq_lv_path[64];
        snprintf(seq_fs_path, sizeof(seq_fs_path), "/spiffs/sprites/%s/frame_001.png", cfg->dir_name);
        snprintf(seq_lv_path, sizeof(seq_lv_path), "S:/spiffs/sprites/%s/frame_001.png", cfg->dir_name);

        if (storage_file_exists(seq_fs_path)) {
            lv_img_header_t header;
            if (lv_img_decoder_get_info(seq_lv_path, &header) == LV_RES_OK) {
                ESP_LOGI(TAG, "Mode: Sequence frames (%s)", cfg->name);
            } else {
                ESP_LOGE(TAG, "Failed to decode frame_001 at %s", seq_lv_path);
            }
            goto setup_viewport;
        }
    }

    if (storage_file_exists(fs_path)) {
        FILE *f = fopen(fs_path, "rb");
        if (f) {
            uint8_t magic[4];
            fread(magic, 1, 4, f);
            fclose(f);
            if (magic[0] != 0x89 || magic[1] != 0x50) {
                ESP_LOGE(TAG, "sprite.png exists but INVALID PNG header!");
                goto show_placeholder;
            }
        }
        ESP_LOGI(TAG, "Mode: Sprite sheet (/spiffs/sprite.png)");
    } else {
        goto show_placeholder;
    }

setup_viewport:
    if (!pet_img) {
        lv_obj_t *viewport = lv_obj_create(pet_page);
        lv_obj_set_size(viewport, DISPLAY_W, DISPLAY_H);
        lv_obj_set_style_clip_corner(viewport, true, 0);
        lv_obj_set_style_border_width(viewport, 0, 0);
        lv_obj_set_style_bg_opa(viewport, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(viewport, 0, 0);
        lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(viewport, LV_ALIGN_CENTER, 0, -20);

        pet_img = lv_img_create(viewport);
        lv_img_set_src(pet_img, lv_path);
        lv_img_set_zoom(pet_img, ZOOM_LEVEL);
        lv_obj_align(pet_img, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_img_set_src(pet_img, lv_path);
    }
    
    lv_obj_clear_flag(lv_obj_get_parent(pet_img), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
    return;

show_placeholder:
    if (pet_img) lv_obj_add_flag(lv_obj_get_parent(pet_img), LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
}

void pet_ui_refresh(void) {
    lv_img_cache_invalidate_src(NULL);
    check_sprite_image();
}

/* 动画定时器回调 */
static void anim_timer_cb(lv_timer_t *timer) {
    const PetAnimConfig *cfg = pet_anim_get_config(current_state);
    if (!cfg) return;

    current_frame++;
    if (current_frame > cfg->frames) {
        current_frame = 1;
    }

    if (frame_label) {
        lv_label_set_text_fmt(frame_label, "Frame: %d / %d", current_frame, cfg->frames);
    }

    if (pet_img && !lv_obj_has_flag(lv_obj_get_parent(pet_img), LV_OBJ_FLAG_HIDDEN)) {
        char seq_fs_path[64];
        char seq_lv_path[64];
        snprintf(seq_fs_path, sizeof(seq_fs_path), "/spiffs/sprites/%s/frame_%03d.png", cfg->dir_name, current_frame);
        snprintf(seq_lv_path, sizeof(seq_lv_path), "S:/spiffs/sprites/%s/frame_%03d.png", cfg->dir_name, current_frame);

        if (storage_file_exists(seq_fs_path)) {
            lv_img_cache_invalidate_src(NULL); 
            lv_img_set_src(pet_img, seq_lv_path);
            lv_obj_set_pos(pet_img, 0, 0);
        } else {
            int32_t frame_idx = current_frame - 1;
            int32_t x_px = -(frame_idx * FRAME_W * ZOOM_LEVEL / 256);
            int32_t y_px = -(cfg->row * FRAME_H * ZOOM_LEVEL / 256);
            lv_obj_set_pos(pet_img, x_px, y_px);
        }
        
        if (current_state == PET_ANIM_DEADLOOP) {
            update_deadloop_effect(current_frame);
        } else {
            reset_deadloop_effect();
        }
    } else if (pet_obj) {
        if (current_state == PET_ANIM_DEADLOOP) {
            update_deadloop_effect(current_frame);
            return;
        }
        reset_deadloop_effect();
        int32_t x_off = (current_frame % 2 == 0) ? 2 : -2;
        lv_obj_set_x(pet_obj, x_off);
        lv_color_t color;
        switch(cfg->row) {
            case 0: color = lv_color_hex(0x00C864); break;
            case 1: case 2: color = lv_color_hex(0xB464FF); break;
            case 3: case 4: color = lv_color_hex(0xFFC800); break;
            case 9: color = lv_color_hex(0xFF3030); break;
            default: color = lv_color_hex(0xFF8C00); break;
        }
        lv_obj_set_style_bg_color(pet_obj, color, 0);
    }

    if (timer->period != cfg->interval_ms) {
        lv_timer_set_period(timer, cfg->interval_ms);
    }
}

void pet_ui_init(void) {
    pet_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(pet_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(pet_page, lv_color_hex(0x080810), 0);
    lv_obj_set_style_border_width(pet_page, 0, 0);
    lv_obj_set_style_radius(pet_page, 0, 0);
    lv_obj_add_flag(pet_page, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(pet_page);
    lv_label_set_text(title, "PETDEX ANIMATION");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    pet_obj = lv_obj_create(pet_page);
    lv_obj_set_size(pet_obj, DISPLAY_W, DISPLAY_H);
    lv_obj_set_style_radius(pet_obj, 10, 0);
    lv_obj_set_style_bg_color(pet_obj, lv_color_hex(0x00C864), 0);
    lv_obj_set_style_border_width(pet_obj, 2, 0);
    lv_obj_set_style_border_color(pet_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(pet_obj, LV_ALIGN_CENTER, 0, -20);

    pet_face_label = lv_label_create(pet_obj);
    lv_label_set_text(pet_face_label, "Pixel");
    lv_obj_set_style_text_color(pet_face_label, lv_color_hex(0x080810), 0);
    lv_obj_set_style_text_align(pet_face_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pet_face_label, LV_ALIGN_CENTER, 0, 0);

    for (int i = 0; i < DEADLOOP_PARTICLE_COUNT; i++) {
        deadloop_particles[i] = lv_obj_create(pet_page);
        lv_obj_set_size(deadloop_particles[i], 6, 6);
        lv_obj_set_style_radius(deadloop_particles[i], 1, 0);
        lv_obj_set_style_border_width(deadloop_particles[i], 0, 0);
        lv_obj_set_style_bg_color(deadloop_particles[i], lv_color_hex(0xFF8C00), 0);
        lv_obj_add_flag(deadloop_particles[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(deadloop_particles[i], LV_ALIGN_CENTER, 0, -20);
    }

    state_label = lv_label_create(pet_page);
    lv_label_set_text(state_label, "State: Idle");
    lv_obj_set_style_text_color(state_label, lv_color_hex(0xA0A0AA), 0);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 50);

    frame_label = lv_label_create(pet_page);
    lv_label_set_text(frame_label, "Frame: 1 / 6");
    lv_obj_set_style_text_color(frame_label, lv_color_hex(0x64B4FF), 0);
    lv_obj_align(frame_label, LV_ALIGN_CENTER, 0, 75);

    deadloop_label = lv_label_create(pet_page);
    lv_label_set_text(deadloop_label, "while(1) detected\nplease feed watchdog");
    lv_obj_set_style_text_color(deadloop_label, lv_color_hex(0xFFE66D), 0);
    lv_obj_set_style_text_align(deadloop_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(deadloop_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(deadloop_label, LV_ALIGN_CENTER, 0, 105);

    lv_obj_t *hint = lv_label_create(pet_page);
    lv_label_set_text(hint, "Send 'petdex <state>' to change\nPress BOOT key to return");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x464650), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -40);

    anim_timer = lv_timer_create(anim_timer_cb, 150, NULL);
    lv_timer_pause(anim_timer);
    check_sprite_image();
    ESP_LOGI(TAG, "Petdex UI initialized");
}

void pet_ui_show(bool show) {
    if (!pet_page) return;
    if (show) {
        check_sprite_image();
        lv_obj_clear_flag(pet_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(pet_page);
        if (anim_timer) lv_timer_resume(anim_timer);
    } else {
        lv_obj_add_flag(pet_page, LV_OBJ_FLAG_HIDDEN);
        if (anim_timer) lv_timer_pause(anim_timer);
    }
}

void pet_ui_set_state(PetAnimState state) {
    if (state >= PET_ANIM_MAX) return;
    current_state = state;
    current_frame = 1;
    const PetAnimConfig *cfg = pet_anim_get_config(state);
    if (cfg && state_label) {
        lv_label_set_text_fmt(state_label, "State: %s", cfg->name);
        if (anim_timer) lv_timer_set_period(anim_timer, cfg->interval_ms);
    }
    if (state == PET_ANIM_DEADLOOP) update_deadloop_effect(current_frame);
    else reset_deadloop_effect();
}

void pet_ui_set_state_by_name(const char *name) {
    PetAnimState state = pet_anim_get_state_by_name(name);
    pet_ui_set_state(state);
}
