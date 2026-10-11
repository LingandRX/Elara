/**
 * @file buddy_drawer.c
 */
#include "buddy_drawer.h"
#include "buddy_theme.h"
#include "buddy/buddy_state.h"
#include "battery/battery_monitor.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "BUDDY_DRAWER";

#define DRAWER_H        232
#define DRAWER_RADIUS   12
#define INFO_ROWS       5

static lv_obj_t *s_scrim = NULL;
static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_subtitle = NULL;
static lv_obj_t *s_row_val[INFO_ROWS] = {0};
static bool s_open = false;

static const char *ROW_KEYS[INFO_ROWS] = { "电量", "网络", "存储", "运行", "固件" };

static void panel_anim_cb(void *obj, int32_t y) {
    lv_obj_set_y((lv_obj_t *)obj, y);
}

void buddy_drawer_init(lv_obj_t *parent) {
    if (!parent) { ESP_LOGE(TAG, "null parent"); return; }
    if (s_scrim) return;   /* 只允许初始化一次 */

    s_scrim = lv_obj_create(parent);
    lv_obj_set_size(s_scrim, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_scrim, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_scrim, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_scrim, 0, 0);
    lv_obj_set_style_radius(s_scrim, 0, 0);
    lv_obj_set_style_pad_all(s_scrim, 0, 0);
    lv_obj_align(s_scrim, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);

    s_panel = lv_obj_create(s_scrim);
    lv_obj_set_size(s_panel, SCREEN_W, DRAWER_H);
    lv_obj_set_style_bg_color(s_panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, DRAWER_RADIUS, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_panel, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *handle = lv_obj_create(s_panel);
    lv_obj_set_size(handle, 28, 3);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, COLOR_LINE, 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 7);

    s_title = lv_label_create(s_panel);
    lv_label_set_text(s_title, "--");
    lv_obj_set_style_text_font(s_title, FONT_MD, 0);
    lv_obj_set_style_text_color(s_title, COLOR_TEXT, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 24);

    s_subtitle = lv_label_create(s_panel);
    lv_label_set_text(s_subtitle, "--");
    lv_obj_set_style_text_font(s_subtitle, FONT_SM, 0);
    lv_obj_set_style_text_color(s_subtitle, COLOR_TEXT_DIM, 0);
    lv_obj_align(s_subtitle, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 43);

    lv_obj_t *sep = lv_obj_create(s_panel);
    lv_obj_set_size(sep, SCREEN_W - 2 * PAGE_PAD_X, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 58);

    for (int i = 0; i < INFO_ROWS; i++) {
        lv_obj_t *k = lv_label_create(s_panel);
        lv_label_set_text(k, ROW_KEYS[i]);
        lv_obj_set_style_text_font(k, FONT_SM, 0);
        lv_obj_set_style_text_color(k, COLOR_TEXT_DIM, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 66 + i * 21);

        s_row_val[i] = lv_label_create(s_panel);
        lv_label_set_text(s_row_val[i], "--");
        lv_obj_set_style_text_font(s_row_val[i], FONT_SM, 0);
        lv_obj_set_style_text_color(s_row_val[i], COLOR_TEXT, 0);
        lv_obj_align(s_row_val[i], LV_ALIGN_TOP_RIGHT, -PAGE_PAD_X, 66 + i * 21);
    }
    ESP_LOGI(TAG, "drawer initialized");
}

void buddy_drawer_refresh(void) {
    if (!s_open) return;

    int h = 0, m = 0, sec = 0, y = 0, mo = 0, d = 0, dow = 0;
    if (buddy_get_local_time(&h, &m, &sec, &y, &mo, &d, &dow)) {
        static const char *WD[] = { "日", "一", "二", "三", "四", "五", "六" };
        lv_label_set_text_fmt(s_title, "%d月%d日 周%s", mo, d, WD[dow % 7]);
        lv_label_set_text_fmt(s_subtitle, "%d · %02d:%02d", y, h, m);
    } else {
        lv_label_set_text(s_title, "时间未同步");
        lv_label_set_text(s_subtitle, "--");
    }

    lv_label_set_text_fmt(s_row_val[0], "%d%%%s",
        battery_get_percentage(), battery_is_charging() ? " 充电中" : "");

    char ssid[33] = {0};
    if (wifi_manager_get_connected_ssid(ssid, sizeof(ssid))) {
        lv_label_set_text_fmt(s_row_val[1], "%s  %d/3", ssid, wifi_manager_get_signal_level());
    } else {
        lv_label_set_text(s_row_val[1], "未连接");
    }

    size_t total = 0, used = 0;
    if (storage_get_info(&total, &used) == ESP_OK && total > 0) {
        lv_label_set_text_fmt(s_row_val[2], "%.1f / %.1f MB",
            (double)used / 1048576.0, (double)total / 1048576.0);
    } else {
        lv_label_set_text(s_row_val[2], "--");
    }

    uint32_t up_s = (uint32_t)(esp_timer_get_time() / 1000000);
    lv_label_set_text_fmt(s_row_val[3], "%lu小时%lu分",
        (unsigned long)(up_s / 3600), (unsigned long)((up_s / 60) % 60));
    lv_label_set_text(s_row_val[4], "v1.0.0");
}

void buddy_drawer_open(void) {
    if (s_open) { buddy_drawer_refresh(); return; }
    s_open = true;
    lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_scrim);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_panel);
    lv_anim_set_exec_cb(&a, panel_anim_cb);
    lv_anim_set_values(&a, DRAWER_H, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_start(&a);

    buddy_drawer_refresh();
}

void buddy_drawer_close(void) {
    if (!s_open) return;
    s_open = false;
    lv_anim_del(s_panel, panel_anim_cb);          /* 停止未完成的滑入动画 */
    lv_obj_set_y(s_panel, 0);                      /* 复位到完全展开位置 */
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
}

bool buddy_drawer_is_open(void) { return s_open; }
