/**
 * @file lvgl_widgets.c
 * LVGL 自定义控件实现
 */

#include "lvgl_widgets.h"
#include "lvgl.h"
#include <string.h>

/* 颜色定义 */
#define COLOR_USER_BUB     lv_color_hex(0x007ACC)      /* 用户蓝色 */
#define COLOR_AI_BUB       lv_color_hex(0x2D2D37)      /* AI 深灰 */
#define COLOR_SYS_BUB      lv_color_hex(0x464650)       /* 系统灰 */
#define COLOR_TEXT         lv_color_hex(0xFFFFFF)       /* 白色文字 */
#define COLOR_TEXT_DIM     lv_color_hex(0xA0A0AA)       /* 暗淡文字 */
#define COLOR_LABEL_AI     lv_color_hex(0x64B4FF)       /* AI 标签色 */
#define COLOR_LABEL_USER   lv_color_hex(0x78DCB4)       /* 用户标签色 */

/**
 * 创建消息气泡
 */
lv_obj_t *lvgl_create_message_bubble(lv_obj_t *parent, const char *role, const char *text) {
    if (!parent || !role || !text) return NULL;

    /* 创建气泡容器 */
    lv_obj_t *bubble = lv_obj_create(parent);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, 8, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_width(bubble, lv_obj_get_width(parent) - 16);

    /* 根据角色设置颜色和对齐 */
    lv_color_t bubble_color;
    const char *role_text;
    lv_color_t role_color;

    if (strcmp(role, "user") == 0) {
        bubble_color = COLOR_USER_BUB;
        role_text = "You";
        role_color = COLOR_LABEL_USER;
        lv_obj_align(bubble, LV_ALIGN_TOP_RIGHT, -4, 0);
    } else if (strcmp(role, "ai") == 0) {
        bubble_color = COLOR_AI_BUB;
        role_text = "AI";
        role_color = COLOR_LABEL_AI;
        lv_obj_align(bubble, LV_ALIGN_TOP_LEFT, 4, 0);
    } else {
        bubble_color = COLOR_SYS_BUB;
        role_text = "System";
        role_color = COLOR_TEXT_DIM;
        lv_obj_align(bubble, LV_ALIGN_TOP_MID, 0, 0);
    }

    lv_obj_set_style_bg_color(bubble, bubble_color, 0);

    /* 角色标签 */
    lv_obj_t *role_label = lv_label_create(bubble);
    lv_label_set_text(role_label, role_text);
    lv_obj_set_style_text_color(role_label, role_color, 0);
    lv_obj_set_style_text_font(role_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(role_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 消息文本 */
    lv_obj_t *msg_text = lv_label_create(bubble);
    lv_label_set_text(msg_text, text);
    lv_obj_set_style_text_color(msg_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(msg_text, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_text, lv_obj_get_width(bubble) - 16);
    lv_obj_align(msg_text, LV_ALIGN_TOP_LEFT, 0, 16);

    /* 调整气泡高度 */
    lv_obj_update_layout(bubble);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    return bubble;
}

/**
 * 创建状态指示器
 */
lv_obj_t *lvgl_create_status_indicator(lv_obj_t *parent, const char *state, lv_color_t color) {
    if (!parent || !state) return NULL;

    /* 创建指示器容器 */
    lv_obj_t *indicator = lv_obj_create(parent);
    lv_obj_set_size(indicator, 120, 28);
    lv_obj_set_style_bg_color(indicator, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_radius(indicator, 4, 0);
    lv_obj_set_style_pad_all(indicator, 4, 0);
    lv_obj_set_style_border_width(indicator, 0, 0);

    /* 状态圆点 */
    lv_obj_t *dot = lv_obj_create(indicator);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_radius(dot, 6, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

    /* 状态文本 */
    lv_obj_t *label = lv_label_create(indicator);
    lv_label_set_text(label, state);
    lv_obj_set_style_text_color(label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 22, 0);

    return indicator;
}

/**
 * 创建动画标签
 */
lv_obj_t *lvgl_create_animated_label(lv_obj_t *parent, const char *text) {
    if (!parent || !text) return NULL;

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    /* 添加淡入动画 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 300);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    return label;
}

/**
 * 创建进度条
 */
lv_obj_t *lvgl_create_progress_bar(lv_obj_t *parent) {
    if (!parent) return NULL;

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 150, 8);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2D2D37), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x007ACC), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);

    return bar;
}
