/**
 * @file pet_ui.c
 * Petdex 动画页面实现
 */

#include "pet_ui.h"
#include "storage_manager.h"
#include "esp_log.h"
#include <stdio.h>

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
static uint8_t current_frame = 0;

/* 精灵图配置 (Petdex 标准规格) */
#define SPRITE_PATH "S:/spiffs/sprite.png"
#define FRAME_W 192
#define FRAME_H 208

/* 缩放配置：屏幕宽度 170，建议显示宽度 160 */
#define DISPLAY_W 160
#define DISPLAY_H 173  /* (208 * 160) / 192 */
#define ZOOM_LEVEL ((DISPLAY_W * 256) / FRAME_W)

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

    if (pet_img && !lv_obj_has_flag(pet_img, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_x(pet_img, frame < 4 ? shake : 0);
        lv_obj_set_style_opa(pet_img, blast ? LV_OPA_30 : LV_OPA_COVER, 0);
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

    if (pet_img && !lv_obj_has_flag(pet_img, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_style_opa(pet_img, LV_OPA_COVER, 0);
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
    if (storage_file_exists("/spiffs/sprite.png")) {
        if (!pet_img) {
            pet_img = lv_img_create(pet_page);
            lv_obj_set_size(pet_img, DISPLAY_W, DISPLAY_H);
            lv_img_set_zoom(pet_img, ZOOM_LEVEL);
            lv_obj_align(pet_img, LV_ALIGN_CENTER, 0, -20);
        }
        lv_img_set_src(pet_img, SPRITE_PATH);
        lv_obj_clear_flag(pet_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Sprite sheet found and loaded");
    } else {
        if (pet_img) lv_obj_add_flag(pet_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pet_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 动画定时器回调 */
static void anim_timer_cb(lv_timer_t *timer) {
    const PetAnimConfig *cfg = pet_anim_get_config(current_state);
    if (!cfg) return;

    /* 更新帧 */
    current_frame++;
    if (current_frame >= cfg->frames) {
        current_frame = 0;
    }

    /* 更新 UI */
    if (frame_label) {
        lv_label_set_text_fmt(frame_label, "Frame: %d / %d", current_frame + 1, cfg->frames);
    }

    /* 处理精灵图动画 */
    if (pet_img && !lv_obj_has_flag(pet_img, LV_OBJ_FLAG_HIDDEN)) {
        /* 设置偏移以显示精灵图的特定部分 (假设水平排列帧，垂直排列动作) */
        lv_img_set_offset_x(pet_img, -(current_frame * FRAME_W));
        lv_img_set_offset_y(pet_img, -(cfg->row * FRAME_H));
        
        if (current_state == PET_ANIM_DEADLOOP) {
            update_deadloop_effect(current_frame);
        } else {
            reset_deadloop_effect();
        }
    } else if (pet_obj) {
        /* 模拟动画效果：让 pet_obj 左右晃动或改变透明度 */
        if (current_state == PET_ANIM_DEADLOOP) {
            update_deadloop_effect(current_frame);
            return;
        }

        reset_deadloop_effect();
        int32_t x_off = (current_frame % 2 == 0) ? 2 : -2;
        lv_obj_set_x(pet_obj, x_off);
        
        /* 根据行号改变颜色模拟不同状态 */
        lv_color_t color;
        switch(cfg->row) {
            case 0: color = lv_color_hex(0x00C864); break; // Idle
            case 1: 
            case 2: color = lv_color_hex(0xB464FF); break; // Run
            case 3: 
            case 4: color = lv_color_hex(0xFFC800); break; // Wave/Jump
            case 9: color = lv_color_hex(0xFF3030); break; // Deadloop
            default: color = lv_color_hex(0xFF8C00); break;
        }
        lv_obj_set_style_bg_color(pet_obj, color, 0);
    }

    /* 更新定时器周期（如果配置改变） */
    if (timer->period != cfg->interval_ms) {
        lv_timer_set_period(timer, cfg->interval_ms);
    }
}

void pet_ui_init(void) {
    ESP_LOGI(TAG, "Initializing Petdex UI...");

    /* 创建 Petdex 页面容器 */
    pet_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(pet_page, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(pet_page, lv_color_hex(0x080810), 0);
    lv_obj_set_style_border_width(pet_page, 0, 0);
    lv_obj_set_style_radius(pet_page, 0, 0);
    lv_obj_add_flag(pet_page, LV_OBJ_FLAG_HIDDEN); /* 默认隐藏 */

    /* 标题 */
    lv_obj_t *title = lv_label_create(pet_page);
    lv_label_set_text(title, "PETDEX ANIMATION");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Pet 占位符对象 */
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

    /* 状态名称标签 */
    state_label = lv_label_create(pet_page);
    lv_label_set_text(state_label, "State: Idle");
    lv_obj_set_style_text_color(state_label, lv_color_hex(0xA0A0AA), 0);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 50);

    /* 帧信息标签 */
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

    /* 提示文本 */
    lv_obj_t *hint = lv_label_create(pet_page);
    lv_label_set_text(hint, "Send 'petdex <state>' to change\nPress BOOT key to return");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x464650), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -40);

    /* 创建动画定时器 */
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
    current_frame = 0;
    
    const PetAnimConfig *cfg = pet_anim_get_config(state);
    if (cfg && state_label) {
        lv_label_set_text_fmt(state_label, "State: %s (Row %d)", cfg->name, cfg->row);
        if (anim_timer) {
            lv_timer_set_period(anim_timer, cfg->interval_ms);
        }
    }

    if (state == PET_ANIM_DEADLOOP) {
        update_deadloop_effect(current_frame);
    } else {
        reset_deadloop_effect();
    }
}

void pet_ui_set_state_by_name(const char *name) {
    PetAnimState state = pet_anim_get_state_by_name(name);
    pet_ui_set_state(state);
}
