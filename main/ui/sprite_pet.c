/**
 * @file sprite_pet.c
 * 精灵图宠物组件实现
 * 渲染逻辑参考 pet_ui.c：支持精灵图 (/spiffs/sprite.png) 与序列帧 (/spiffs/sprites/<dir>/) 两种模式
 */

#include "sprite_pet.h"
#include "storage_manager.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "SPRITE_PET";

/* 精灵图规格 (96x104) */
#define SPRITE_SHEET_FS   "/spiffs/sprite.png"
#define SPRITE_SHEET_LV   "S:/spiffs/sprite.png"
#define FRAME_W           96
#define FRAME_H           104
/* 主页精灵图缩放：453 → 约 170x184px (偏大, 超出 170 高动画区并遮挡时钟/电池)
 * 320 → 约 120x130px (适中)。显示宽高 = FRAME_W/H * SPRITE_ZOOM / 256，可按需调整。 */
#define SPRITE_ZOOM       320

struct SpritePet {
    lv_obj_t      *viewport;   /* 视口容器 (裁剪) */
    lv_obj_t      *img;        /* 图片对象 */
    PetAnimState   state;      /* 当前动画状态 */
    uint8_t        frame;      /* 当前帧 (1-based) */
    uint32_t       last_frame_ms; /* 上次换帧时间戳 (lv_tick_get) */
    char           src_path[64]; /* 序列帧 LVGL 路径 (lv_img_set_src 保存指针, 需持久) */
    bool           seq_mode;   /* true=序列帧, false=精灵图 */
    bool           ready;      /* 是否有可用图片资源 */
};

/* 检查并加载当前状态对应的图片资源 */
static void sprite_pet_load_image(SpritePet *sp) {
    if (!sp) return;
    sp->ready = false;

    const PetAnimConfig *cfg = pet_anim_get_config(sp->state);
    if (cfg) {
        char seq_fs_path[64];
        snprintf(seq_fs_path, sizeof(seq_fs_path),
                 "/spiffs/sprites/%s/frame_001.png", cfg->dir_name);
        if (storage_file_exists(seq_fs_path)) {
            snprintf(sp->src_path, sizeof(sp->src_path),
                     "S:/spiffs/sprites/%s/frame_001.png", cfg->dir_name);
            lv_img_header_t header;
            if (lv_img_decoder_get_info(sp->src_path, &header) == LV_RES_OK) {
                sp->seq_mode = true;
                lv_img_set_src(sp->img, sp->src_path);
                sp->ready = true;
                return;
            }
            ESP_LOGE(TAG, "Failed to decode frame_001 at %s", sp->src_path);
        }
    }

    if (storage_file_exists(SPRITE_SHEET_FS)) {
        FILE *f = fopen(SPRITE_SHEET_FS, "rb");
        if (f) {
            uint8_t magic[4];
            fread(magic, 1, 4, f);
            fclose(f);
            if (magic[0] != 0x89 || magic[1] != 0x50) {
                ESP_LOGE(TAG, "sprite.png invalid PNG header!");
                return;
            }
        }
        sp->seq_mode = false;
        lv_img_set_src(sp->img, SPRITE_SHEET_LV);
        sp->ready = true;
    }
}

/* 更新当前帧的图片源/位置 */
static void sprite_pet_update_src(SpritePet *sp) {
    if (!sp || !sp->img) return;
    const PetAnimConfig *cfg = pet_anim_get_config(sp->state);
    if (!cfg) return;

    if (sp->seq_mode) {
        snprintf(sp->src_path, sizeof(sp->src_path),
                 "S:/spiffs/sprites/%s/frame_%03d.png", cfg->dir_name, sp->frame);
        lv_img_cache_invalidate_src(NULL);
        lv_img_set_src(sp->img, sp->src_path);
        lv_obj_set_pos(sp->img, 0, 0);
    } else {
        int32_t frame_idx = sp->frame - 1;
        int32_t x_px = -(frame_idx * FRAME_W * SPRITE_ZOOM / 256);
        int32_t y_px = -(cfg->row * FRAME_H * SPRITE_ZOOM / 256);
        lv_obj_set_pos(sp->img, x_px, y_px);
    }
}

/* 推进动画：按帧间隔自动换帧 (由渲染循环调用, 隐藏时不推进) */
void sprite_pet_update(SpritePet *sp) {
    if (!sp || !sp->viewport) return;
    if (lv_obj_has_flag(sp->viewport, LV_OBJ_FLAG_HIDDEN)) return;
    const PetAnimConfig *cfg = pet_anim_get_config(sp->state);
    if (!cfg) return;

    uint32_t now = lv_tick_get();
    if ((int32_t)(now - sp->last_frame_ms) < (int32_t)cfg->interval_ms) return;
    sp->last_frame_ms = now;

    sp->frame++;
    if (sp->frame > cfg->frames) sp->frame = 1;
    sprite_pet_update_src(sp);
}

SpritePet *sprite_pet_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h) {
    if (!parent) return NULL;
    SpritePet *sp = (SpritePet *)lv_mem_alloc(sizeof(SpritePet));
    if (!sp) return NULL;
    memset(sp, 0, sizeof(*sp));
    sp->state = PET_ANIM_IDLE;
    sp->frame = 1;

    /* 视口容器 */
    sp->viewport = lv_obj_create(parent);
    lv_obj_set_size(sp->viewport, w, h);
    lv_obj_set_style_clip_corner(sp->viewport, true, 0);
    lv_obj_set_style_border_width(sp->viewport, 0, 0);
    lv_obj_set_style_bg_opa(sp->viewport, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(sp->viewport, 0, 0);
    lv_obj_clear_flag(sp->viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sp->viewport, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(sp->viewport, LV_OBJ_FLAG_HIDDEN);

    /* 图片对象 */
    sp->img = lv_img_create(sp->viewport);
    lv_img_set_zoom(sp->img, SPRITE_ZOOM);
    lv_obj_align(sp->img, LV_ALIGN_CENTER, 0, 0);

    /* 初始资源检查 (存储可能未挂载, 挂载后由 sprite_pet_reload 刷新) */
    sp->last_frame_ms = lv_tick_get();
    sprite_pet_load_image(sp);
    ESP_LOGI(TAG, "Sprite pet created (%ux%u)", (unsigned)w, (unsigned)h);
    return sp;
}

void sprite_pet_delete(SpritePet *sp) {
    if (!sp) return;
    if (sp->viewport) lv_obj_del(sp->viewport);
    lv_mem_free(sp);
}

void sprite_pet_set_state(SpritePet *sp, PetAnimState state) {
    if (!sp) return;
    if (state >= PET_ANIM_MAX) return;
    if (state == sp->state) return;  /* 状态未变化, 跳过重载 */

    sp->state = state;
    sp->frame = 1;
    sp->last_frame_ms = lv_tick_get();
    /* 每种状态对应不同资源目录, 切换状态时重新检测 */
    sprite_pet_load_image(sp);
    if (sp->ready) sprite_pet_update_src(sp);
}

void sprite_pet_set_visible(SpritePet *sp, bool visible) {
    if (!sp || !sp->viewport) return;
    if (visible) {
        if (!sp->ready) return;  /* 无资源时无法显示 */
        lv_obj_clear_flag(sp->viewport, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(sp->viewport, LV_OBJ_FLAG_HIDDEN);
    }
}

bool sprite_pet_is_visible(const SpritePet *sp) {
    return sp && sp->viewport &&
           !lv_obj_has_flag(sp->viewport, LV_OBJ_FLAG_HIDDEN);
}

bool sprite_pet_ready(const SpritePet *sp) {
    return sp && sp->ready;
}

void sprite_pet_reload(SpritePet *sp) {
    if (!sp) return;
    sprite_pet_load_image(sp);
    if (sp->ready) sprite_pet_update_src(sp);
}
