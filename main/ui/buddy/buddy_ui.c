/**
 * @file buddy_ui.c
 * Buddy 主页面 UI 实现
 * Buddy 主界面实现
 */

#include "buddy_theme.h"
#include "buddy_strings.h"
#include "buddy_ui.h"
#include "buddy_anim.h"
#include "buddy_drawer.h"
#include "wifi_ui.h"
#include "buddy/buddy_state.h"
#include "buddy/buddy_stats.h"
#include "esp_log.h"
#include "esp_system.h"
#include "input/lvgl_touch.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BUDDY_UI";

/* ============ 布局常量 ============ */
#define ANIM_AREA_H     170     /* 角色动画区域高度 */
#define STATUS_AREA_H   150     /* 状态区域高度 */
#define STATUS_AREA_Y   170     /* 状态区域 Y 偏移 */
#define PAD             4       /* 通用内边距 */

/* ============ 静态 UI 元素 ============ */
static lv_obj_t *s_main_scr = NULL;
static lv_obj_t *s_container = NULL;

/* NORMAL 模式 */
static lv_obj_t *s_normal_page = NULL;
static lv_obj_t *s_anim_canvas = NULL;
static lv_obj_t *s_clock_label = NULL;
static lv_obj_t *s_wifi_label = NULL;
static int s_last_wifi_level = -1;   /* 诊断：上次设置的信号等级 */
static lv_obj_t *s_battery_label = NULL;
static lv_obj_t *s_persona_label = NULL;
static lv_obj_t *s_mood_dots[3] = {NULL, NULL, NULL};

/* 渲染去重缓存：主循环每帧都会调用 set_persona / set_wifi，
 * 若不做变化判断会每帧触发 label 重排+重绘，拖慢 lv_task_handler */
static int  s_last_persona   = -1;
static int  s_last_wifi_en   = -1;
static int  s_last_wifi_lvl  = -1;

/* INFO 模式 */
static lv_obj_t *s_info_page = NULL;
static lv_obj_t *s_info_title = NULL;
static lv_obj_t *s_info_page_idx = NULL;
static lv_obj_t *s_info_content = NULL;

/* 菜单（全屏抽屉） */
static lv_obj_t *s_menu_overlay = NULL;
static lv_obj_t *s_menu_panel = NULL;
static lv_obj_t *s_menu_items[BUDDY_MENU_MAX];
static lv_obj_t *s_menu_labels[BUDDY_MENU_MAX];
static lv_obj_t *s_menu_bars[BUDDY_MENU_MAX];    /* 选中项左侧 3px 琥珀竖条 */

/* 设置（全屏页） */
static lv_obj_t *s_settings_overlay = NULL;
static lv_obj_t *s_settings_panel = NULL;
static lv_obj_t *s_setting_items[BUDDY_SET_MAX];
static lv_obj_t *s_setting_labels[BUDDY_SET_MAX];
static lv_obj_t *s_setting_vals[BUDDY_SET_MAX];
static lv_obj_t *s_setting_bars[BUDDY_SET_MAX];  /* 选中项左侧 3px 琥珀竖条 */

/* 恢复出厂确认弹窗 */
static lv_obj_t *s_reset_overlay = NULL;
static lv_obj_t *s_reset_panel = NULL;

/* 抽屉/页面布局常量 */
#define MENU_PANEL_H    184
#define MENU_ITEM_H     34
#define MENU_ITEM_GAP   4
#define MENU_ITEM_X     13
#define SET_ITEM_H      24
#define SET_ITEM_X      13


/* ============ 状态变量 ============ */
static BuddyMode s_mode = BUDDY_MODE_NORMAL;
static bool s_ui_visible = false;
static bool s_menu_visible = false;
static bool s_settings_visible = false;
static InfoPageIdx s_info_idx = INFO_PAGE_ABOUT;
static int s_menu_selected = 0;

/* 时钟 */
static int s_clock_h = 12;
static int s_clock_m = 0;
static int s_clock_s = 0;

/* 设置状态 */
static int  s_brightness_pct = 80;
static bool s_set_sound = true;
static bool s_set_wifi = true;
static bool s_set_info_panel = true;
static bool s_set_rotate = false;
static bool s_set_auto_sleep = false;

/* 外部关闭回调 */
static BuddyOverlayCloseCb s_menu_close_cb = NULL;
static BuddyOverlayCloseCb s_settings_close_cb = NULL;

/* 菜单项点击动作回调 */
static BuddyMenuActionCb s_menu_action_cb = NULL;

/* 设置项点击动作回调 */
static BuddySettingActionCb s_settings_action_cb = NULL;

/* 恢复出厂确认回调（确认时触发，由 main.c 执行重启） */
static BuddyResetConfirmCb s_reset_confirm_cb = NULL;

/* ============ 菜单文本 ============ */
static const char *s_menu_texts[BUDDY_MENU_MAX] = {
    "网络",
    "设置",
    "关于",
    "关机",
};

/* ============ 设置项文本 ============ */
static const char *s_setting_texts[BUDDY_SET_MAX] = {
    "亮度",
    "信息面板",
    "旋转",
    "Wi-Fi",
    "声音",
    "自动休眠",
    "恢复出厂",
};

/* ============ Info 页面标题 ============ */
static const char *s_info_titles[INFO_PAGE_MAX] = {
    "关于",
};

/* ============ 辅助函数 ============ */

static inline void obj_set_pad_all(lv_obj_t *obj, lv_coord_t pad) {
    lv_obj_set_style_pad_left(obj, pad, 0);
    lv_obj_set_style_pad_right(obj, pad, 0);
    lv_obj_set_style_pad_top(obj, pad, 0);
    lv_obj_set_style_pad_bottom(obj, pad, 0);
}

static inline void obj_set_hidden(lv_obj_t *obj, bool hidden) {
    if (!obj) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void switch_mode_pages(void) {
    obj_set_hidden(s_normal_page, s_mode != BUDDY_MODE_NORMAL);
    obj_set_hidden(s_info_page,   s_mode != BUDDY_MODE_INFO);
}

/* ============ NORMAL 页面创建 ============ */

static void create_normal_page(void) {
    s_normal_page = lv_obj_create(s_container);
    lv_obj_set_size(s_normal_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_normal_page, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_normal_page, 0, 0);
    lv_obj_set_style_radius(s_normal_page, 0, 0);
    obj_set_pad_all(s_normal_page, 0);
    lv_obj_align(s_normal_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_normal_page, LV_OBJ_FLAG_SCROLLABLE);

    /* 角色动画区域 (上半部) */
    s_anim_canvas = lv_obj_create(s_normal_page);
    lv_obj_set_size(s_anim_canvas, SCREEN_W, ANIM_AREA_H);
    lv_obj_set_style_bg_color(s_anim_canvas, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_anim_canvas, 0, 0);
    lv_obj_set_style_radius(s_anim_canvas, 0, 0);
    obj_set_pad_all(s_anim_canvas, 0);
    lv_obj_align(s_anim_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_anim_canvas, LV_OBJ_FLAG_SCROLLABLE);

    /* 时钟标签 */
    s_clock_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_clock_label, "12:00");
    lv_obj_set_style_text_color(s_clock_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_clock_label, BUDDY_FONT_DEFAULT, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_LEFT, 4, 4);

    /* 电池标签（右上角） */
    s_battery_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_battery_label, LV_SYMBOL_CHARGE " 100%");
    lv_obj_set_style_text_color(s_battery_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_battery_label, BUDDY_FONT_DEFAULT, 0);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* WiFi 连接标识（header 顶部，电池左侧） */
    s_wifi_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_wifi_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_wifi_label, BUDDY_FONT_DEFAULT, 0);
    lv_obj_align(s_wifi_label, LV_ALIGN_TOP_RIGHT, -54, 4);

    /* 状态栏分隔线 */
    lv_obj_t *sep = lv_obj_create(s_normal_page);
    lv_obj_set_size(sep, SCREEN_W - 12, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, STATUS_BAR_H - 1);

    /* 状态词 */
    s_persona_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_persona_label, "待机");
    lv_obj_set_style_text_color(s_persona_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_persona_label, FONT_MD, 0);
    lv_obj_align(s_persona_label, LV_ALIGN_TOP_MID, 0, 202);

    /* 心情指示点 */
    for (int i = 0; i < 3; i++) {
        s_mood_dots[i] = lv_obj_create(s_normal_page);
        lv_obj_set_size(s_mood_dots[i], 5, 5);
        lv_obj_set_style_radius(s_mood_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_mood_dots[i], COLOR_LINE, 0);
        lv_obj_set_style_border_width(s_mood_dots[i], 0, 0);
        lv_obj_clear_flag(s_mood_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(s_mood_dots[i], LV_ALIGN_TOP_MID, (i - 1) * 12, 276);
    }

    /* 手势提示 */
    lv_obj_t *hint = lv_label_create(s_normal_page);
    lv_label_set_text(hint, "上滑菜单  下滑信息");
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, FONT_SM, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 292);
}

/* ============ 关于页 ============ */
static void info_long_press_cb(lv_event_t *e);

static void create_info_page(void) {
    s_info_page = lv_obj_create(s_container);
    lv_obj_set_size(s_info_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_info_page, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_info_page, 0, 0);
    lv_obj_set_style_radius(s_info_page, 0, 0);
    obj_set_pad_all(s_info_page, 0);
    lv_obj_align(s_info_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_info_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_info_page, LV_OBJ_FLAG_HIDDEN);

    /* 标题栏：标题 + 右上「完成」 */
    lv_obj_t *title_bar = lv_obj_create(s_info_page);
    lv_obj_set_size(title_bar, SCREEN_W, 30);
    lv_obj_set_style_bg_color(title_bar, COLOR_BG, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    obj_set_pad_all(title_bar, 0);
    lv_obj_align(title_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_info_title = lv_label_create(title_bar);
    lv_label_set_text(s_info_title, "关于");
    lv_obj_set_style_text_color(s_info_title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_info_title, FONT_MD, 0);
    lv_obj_align(s_info_title, LV_ALIGN_LEFT_MID, PAGE_PAD_X, 0);

    s_info_page_idx = lv_label_create(title_bar);   /* 复用为「完成」 */
    lv_label_set_text(s_info_page_idx, "完成");
    lv_obj_set_style_text_color(s_info_page_idx, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(s_info_page_idx, FONT_MD, 0);
    lv_obj_align(s_info_page_idx, LV_ALIGN_RIGHT_MID, -PAGE_PAD_X, 0);
    lv_obj_add_flag(s_info_page_idx, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_info_page_idx, info_long_press_cb, LV_EVENT_CLICKED, NULL);

    /* 标题栏分隔线 */
    lv_obj_t *sep = lv_obj_create(s_info_page);
    lv_obj_set_size(sep, SCREEN_W - 2 * PAGE_PAD_X, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容区域（禁止滚动，避免拖动被当作滚动） */
    s_info_content = lv_label_create(s_info_page);
    lv_obj_set_style_text_color(s_info_content, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_info_content, FONT_SM, 0);
    lv_label_set_long_mode(s_info_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(s_info_content, SCREEN_W - 2 * PAGE_PAD_X, SCREEN_H - 70);
    lv_obj_align(s_info_content, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 38);
    lv_obj_clear_flag(s_info_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_info_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_info_content, info_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
}

/* 关于页：短按「完成」返回；长按内容切换演示模式 */
static void info_long_press_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        /* 「完成」返回主界面 */
        BuddyUIState *ui = buddy_get_ui_state();
        ui->display_mode = DISP_NORMAL;
        buddy_ui_set_mode(BUDDY_MODE_NORMAL);
        buddy_anim_invalidate();
        return;
    }
    if (code != LV_EVENT_LONG_PRESSED) return;
    extern void buddy_set_demo(bool on);
    extern bool buddy_is_demo(void);
    buddy_set_demo(!buddy_is_demo());
    lv_label_set_text(s_info_content, buddy_is_demo() ? "演示模式: 开" : "演示模式: 关");
}

static void update_info_content(void) {
    if (!s_info_content) return;

    lv_label_set_text(s_info_title, s_info_titles[s_info_idx]);
    lv_label_set_text(s_info_content,
        "Elara 桌面伴侣\n"
        "\n"
        "版本   1.0.0\n"
        "LVGL   8.4\n"
        "芯片   ESP32-S3\n"
        "屏幕   170x320\n"
        "\n"
        "按键说明\n"
        "  BOOT 短按   菜单\n"
        "  BOOT 长按   返回\n"
        "  触摸        导航\n"
        "\n"
        "长按本页可切换演示模式");
}

/* ============ 覆盖层外部点击关闭 ============ */

static void overlay_click_close_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    if (target == overlay) {
        /* 点击了 overlay 背景（panel 外部） */
        if (overlay == s_menu_overlay && s_menu_close_cb) {
            buddy_ui_show_menu(false);
            buddy_anim_invalidate();
            s_menu_close_cb();
        }
    }
}

/* 设置页「完成」：关闭设置并同步外部状态 */
static void settings_done_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    buddy_ui_show_settings(false);
    buddy_anim_invalidate();
    if (s_settings_close_cb) s_settings_close_cb();
}

/* ============ INFO 页面滑动 ============ */

/* 驱动层滑动检测回调 (绕过 LVGL 手势系统, 见 lvgl_touch.c) */
void buddy_ui_handle_swipe(lv_dir_t dir) {
    /* WiFi 页面：其列表自身纵向滚动，忽略全局手势 */
    if (wifi_ui_is_visible()) return;
    /* 菜单/设置覆盖层打开时：忽略手势，避免叠加抽屉 */
    if (buddy_ui_is_menu_visible()) return;
    {
        BuddyUIState *ui = buddy_get_ui_state();
        if (ui->settings_open || ui->reset_open) return;
    }
    /* 抽屉打开时：上下滑动关闭 */
    if (buddy_drawer_is_open()) {
        if (dir == LV_DIR_BOTTOM || dir == LV_DIR_TOP) buddy_drawer_close();
        return;
    }
    /* 主界面：下滑出信息面板（抽屉，受设置开关控制），上滑出菜单（复用既有菜单覆盖层，见裁定 R6） */
    if (dir == LV_DIR_BOTTOM) {
        if (buddy_settings_get()->hud) buddy_drawer_open();
    } else if (dir == LV_DIR_TOP) {
        BuddyUIState *ui = buddy_get_ui_state();
        ui->menu_open = true;
        ui->menu_sel = 0;
        buddy_ui_show_menu(true);
    }
}

/* 统一「选中」视觉：PANEL2 底 + 左侧 3px 琥珀竖条（见设计文档 §5.3） */
static void apply_row_selection(lv_obj_t *item, lv_obj_t *bar, lv_obj_t *label,
                                bool selected, bool label_bold_hint) {
    if (!item) return;
    lv_obj_set_style_bg_color(item, selected ? COLOR_PANEL2 : COLOR_PANEL, 0);
    if (bar) {
        if (selected) lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (label) {
        lv_obj_set_style_text_color(label, selected ? COLOR_TEXT : COLOR_TEXT_DIM, 0);
        (void)label_bold_hint;
    }
}

/* ============ 菜单覆盖层创建 ============ */

static void menu_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < BUDDY_MENU_MAX; i++) {
        if (s_menu_items[i] == target) {
            s_menu_selected = i;
            ESP_LOGI(TAG, "Menu clicked: %d", i);
            /* 高亮选中项：PANEL2 底 + 左侧琥珀竖条 */
            for (int j = 0; j < BUDDY_MENU_MAX; j++) {
                apply_row_selection(s_menu_items[j], s_menu_bars[j], s_menu_labels[j], j == i, false);
            }
            /* 触发菜单动作（进入对应子菜单） */
            if (s_menu_action_cb) {
                s_menu_action_cb((BuddyMenuItem)i);
            }
            break;
        }
    }
}

static void create_menu_overlay(void) {
    /* 全屏遮罩：点击面板外关闭 */
    s_menu_overlay = lv_obj_create(s_container);
    lv_obj_set_size(s_menu_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_menu_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_menu_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_menu_overlay, 0, 0);
    lv_obj_set_style_radius(s_menu_overlay, 0, 0);
    obj_set_pad_all(s_menu_overlay, 0);
    lv_obj_align(s_menu_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_menu_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_menu_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_menu_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_menu_overlay, overlay_click_close_cb, LV_EVENT_CLICKED, s_menu_overlay);

    /* 底部抽屉面板（见设计文档 §5.3：从底部滑入，无标题，带拖拽把手） */
    s_menu_panel = lv_obj_create(s_menu_overlay);
    lv_obj_set_size(s_menu_panel, SCREEN_W, MENU_PANEL_H);
    lv_obj_set_style_bg_color(s_menu_panel, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_menu_panel, 0, 0);
    lv_obj_set_style_radius(s_menu_panel, 11, 0);
    obj_set_pad_all(s_menu_panel, 0);
    lv_obj_align(s_menu_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 拖拽把手 */
    lv_obj_t *handle = lv_obj_create(s_menu_panel);
    lv_obj_set_size(handle, 28, 3);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, COLOR_LINE, 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 7);

    /* 菜单项 */
    lv_coord_t y = 22;
    for (int i = 0; i < BUDDY_MENU_MAX; i++) {
        s_menu_items[i] = lv_obj_create(s_menu_panel);
        lv_obj_set_size(s_menu_items[i], SCREEN_W - 2 * MENU_ITEM_X, MENU_ITEM_H);
        lv_obj_set_style_bg_color(s_menu_items[i], COLOR_PANEL, 0);
        lv_obj_set_style_border_width(s_menu_items[i], 0, 0);
        lv_obj_set_style_radius(s_menu_items[i], 8, 0);
        obj_set_pad_all(s_menu_items[i], 0);
        lv_obj_align(s_menu_items[i], LV_ALIGN_TOP_LEFT, MENU_ITEM_X, y);
        lv_obj_add_flag(s_menu_items[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_menu_items[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(s_menu_items[i], menu_event_cb, LV_EVENT_CLICKED, NULL);

        /* 左侧 3px 琥珀竖条（仅选中时显示） */
        s_menu_bars[i] = lv_obj_create(s_menu_items[i]);
        lv_obj_set_size(s_menu_bars[i], 3, 18);
        lv_obj_set_style_radius(s_menu_bars[i], 2, 0);
        lv_obj_set_style_bg_color(s_menu_bars[i], COLOR_ACCENT, 0);
        lv_obj_set_style_border_width(s_menu_bars[i], 0, 0);
        lv_obj_clear_flag(s_menu_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(s_menu_bars[i], LV_ALIGN_LEFT_MID, 6, 0);
        lv_obj_add_flag(s_menu_bars[i], LV_OBJ_FLAG_HIDDEN);

        s_menu_labels[i] = lv_label_create(s_menu_items[i]);
        lv_label_set_text(s_menu_labels[i], s_menu_texts[i]);
        lv_obj_set_style_text_color(s_menu_labels[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(s_menu_labels[i], BUDDY_FONT_DEFAULT, 0);
        lv_obj_align(s_menu_labels[i], LV_ALIGN_LEFT_MID, 16, 0);

        y += MENU_ITEM_H + MENU_ITEM_GAP;
    }

    /* 初始选中第 0 项 */
    for (int i = 0; i < BUDDY_MENU_MAX; i++) {
        apply_row_selection(s_menu_items[i], s_menu_bars[i], s_menu_labels[i], i == 0, false);
    }
}

/* ============ 设置覆盖层创建 ============ */

static void settings_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        if (s_setting_items[i] == target) {
            ESP_LOGI(TAG, "Settings clicked: %d", i);
            for (int j = 0; j < BUDDY_SET_MAX; j++) {
                apply_row_selection(s_setting_items[j], s_setting_bars[j], s_setting_labels[j], j == i, false);
            }
            /* 触发设置动作（执行对应功能） */
            if (s_settings_action_cb) {
                s_settings_action_cb((BuddySettingItem)i);
            }
            break;
        }
    }
}

static void create_settings_overlay(void) {
    /* 全屏页（见设计文档 §5.4：分组 + 右上「完成」） */
    s_settings_overlay = lv_obj_create(s_container);
    lv_obj_set_size(s_settings_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_settings_overlay, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_settings_overlay, 0, 0);
    lv_obj_set_style_radius(s_settings_overlay, 0, 0);
    obj_set_pad_all(s_settings_overlay, 0);
    lv_obj_align(s_settings_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_settings_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* 标题栏：标题 + 右上「完成」 */
    s_settings_panel = lv_obj_create(s_settings_overlay);
    lv_obj_set_size(s_settings_panel, SCREEN_W, 30);
    lv_obj_set_style_bg_color(s_settings_panel, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_settings_panel, 0, 0);
    lv_obj_set_style_radius(s_settings_panel, 0, 0);
    obj_set_pad_all(s_settings_panel, 0);
    lv_obj_align(s_settings_panel, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_settings_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *st = lv_label_create(s_settings_panel);
    lv_label_set_text(st, "设置");
    lv_obj_set_style_text_color(st, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(st, FONT_MD, 0);
    lv_obj_align(st, LV_ALIGN_LEFT_MID, PAGE_PAD_X, 0);

    lv_obj_t *done = lv_label_create(s_settings_panel);
    lv_label_set_text(done, "完成");
    lv_obj_set_style_text_color(done, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(done, FONT_MD, 0);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, -PAGE_PAD_X, 0);
    lv_obj_add_flag(done, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(done, settings_done_cb, LV_EVENT_CLICKED, NULL);

    /* 标题栏分隔线 */
    lv_obj_t *sep = lv_obj_create(s_settings_overlay);
    lv_obj_set_size(sep, SCREEN_W - 2 * PAGE_PAD_X, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 30);

    /* 分组标题：显示 / 连接 / 系统（见设计文档 §5.4） */
    static const char *group_titles[] = { "显示", "连接", "系统" };
    /* 显示: 亮度/信息面板/旋转(0-2)；连接: Wi-Fi/声音(3-4)；系统: 自动休眠/恢复出厂(5-6) */
    static const int    group_at[]     = { 0, 3, 5 };
    static const int    group_count    = 3;

    lv_coord_t y = 38;
    for (int g = 0; g < group_count; g++) {
        lv_obj_t *gt = lv_label_create(s_settings_overlay);
        lv_label_set_text(gt, group_titles[g]);
        lv_obj_set_style_text_color(gt, COLOR_ACCENT, 0);
        lv_obj_set_style_text_font(gt, FONT_SM, 0);
        lv_obj_align(gt, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, y);
        y += 16;

        int n = (g + 1 < group_count) ? (group_at[g + 1] - group_at[g]) : (BUDDY_SET_MAX - group_at[g]);
        for (int k = 0; k < n; k++) {
            int i = group_at[g] + k;
            s_setting_items[i] = lv_obj_create(s_settings_overlay);
            lv_obj_set_size(s_setting_items[i], SCREEN_W - 2 * SET_ITEM_X, SET_ITEM_H);
            lv_obj_set_style_bg_color(s_setting_items[i], COLOR_BG, 0);
            lv_obj_set_style_border_width(s_setting_items[i], 0, 0);
            lv_obj_set_style_radius(s_setting_items[i], 6, 0);
            obj_set_pad_all(s_setting_items[i], 0);
            lv_obj_align(s_setting_items[i], LV_ALIGN_TOP_LEFT, SET_ITEM_X, y);
            lv_obj_add_flag(s_setting_items[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(s_setting_items[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_event_cb(s_setting_items[i], settings_event_cb, LV_EVENT_CLICKED, NULL);

            /* 左侧 3px 琥珀竖条（仅选中时显示） */
            s_setting_bars[i] = lv_obj_create(s_setting_items[i]);
            lv_obj_set_size(s_setting_bars[i], 3, 16);
            lv_obj_set_style_radius(s_setting_bars[i], 2, 0);
            lv_obj_set_style_bg_color(s_setting_bars[i], COLOR_ACCENT, 0);
            lv_obj_set_style_border_width(s_setting_bars[i], 0, 0);
            lv_obj_clear_flag(s_setting_bars[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(s_setting_bars[i], LV_ALIGN_LEFT_MID, 5, 0);
            lv_obj_add_flag(s_setting_bars[i], LV_OBJ_FLAG_HIDDEN);

            s_setting_labels[i] = lv_label_create(s_setting_items[i]);
            lv_label_set_text(s_setting_labels[i], s_setting_texts[i]);
            lv_obj_set_style_text_color(s_setting_labels[i], COLOR_TEXT_DIM, 0);
            lv_obj_set_style_text_font(s_setting_labels[i], FONT_MD, 0);
            lv_obj_align(s_setting_labels[i], LV_ALIGN_LEFT_MID, 14, 0);

            s_setting_vals[i] = lv_label_create(s_setting_items[i]);
            lv_obj_set_style_text_color(s_setting_vals[i], COLOR_TEXT_DIM, 0);
            lv_obj_set_style_text_font(s_setting_vals[i], FONT_MD, 0);
            lv_obj_align(s_setting_vals[i], LV_ALIGN_RIGHT_MID, -6, 0);

            y += SET_ITEM_H;
        }
        y += 8;   /* 分组间距 */
    }

    /* 初始化设置值显示 */
    buddy_ui_settings_set_brightness(s_brightness_pct);
    buddy_ui_settings_set_toggle(BUDDY_SET_INFO_PANEL, s_set_info_panel);
    buddy_ui_settings_set_rotate(0);
    buddy_ui_settings_set_toggle(BUDDY_SET_WIFI, s_set_wifi);
    buddy_ui_settings_set_toggle(BUDDY_SET_SOUND, s_set_sound);
    buddy_ui_settings_set_toggle(BUDDY_SET_AUTO_SLEEP, s_set_auto_sleep);
    lv_label_set_text(s_setting_vals[BUDDY_SET_RESET], "");

    /* 初始选中第 0 项 */
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        apply_row_selection(s_setting_items[i], s_setting_bars[i], s_setting_labels[i], i == 0, false);
    }
}

/* ============ 恢复出厂确认弹窗 ============ */

static void reset_confirm_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    intptr_t which = (intptr_t)lv_event_get_user_data(e);
    buddy_ui_show_reset_confirm(false);
    /* which==1 表示「确认」：通知外部执行；0 表示取消 */
    if (which == 1 && s_reset_confirm_cb) s_reset_confirm_cb();
}

static void create_reset_overlay(void) {
    s_reset_overlay = lv_obj_create(s_container);
    lv_obj_set_size(s_reset_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_reset_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_reset_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_reset_overlay, 0, 0);
    lv_obj_set_style_radius(s_reset_overlay, 0, 0);
    obj_set_pad_all(s_reset_overlay, 0);
    lv_obj_align(s_reset_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_reset_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_reset_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_reset_overlay, LV_OBJ_FLAG_CLICKABLE);

    s_reset_panel = lv_obj_create(s_reset_overlay);
    lv_obj_set_size(s_reset_panel, 140, 116);
    lv_obj_set_style_bg_color(s_reset_panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(s_reset_panel, 0, 0);
    lv_obj_set_style_radius(s_reset_panel, 11, 0);
    obj_set_pad_all(s_reset_panel, 0);
    lv_obj_center(s_reset_panel);
    lv_obj_clear_flag(s_reset_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_reset_panel);
    lv_label_set_text(title, "恢复出厂");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *desc = lv_label_create(s_reset_panel);
    lv_label_set_text(desc, "将清除全部设置与统计");
    lv_obj_set_style_text_color(desc, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(desc, FONT_SM, 0);
    lv_obj_align(desc, LV_ALIGN_TOP_MID, 0, 38);

    /* 取消 / 确认 两个按钮 */
    lv_obj_t *btn_cancel = lv_obj_create(s_reset_panel);
    lv_obj_set_size(btn_cancel, 52, 26);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_BG, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    obj_set_pad_all(btn_cancel, 0);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 14, -14);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_cancel, reset_confirm_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_t *cl = lv_label_create(btn_cancel);
    lv_label_set_text(cl, "取消");
    lv_obj_set_style_text_color(cl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(cl, FONT_MD, 0);
    lv_obj_center(cl);

    lv_obj_t *btn_ok = lv_obj_create(s_reset_panel);
    lv_obj_set_size(btn_ok, 52, 26);
    lv_obj_set_style_bg_color(btn_ok, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 6, 0);
    obj_set_pad_all(btn_ok, 0);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
    lv_obj_add_flag(btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_ok, reset_confirm_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    lv_obj_t *ol = lv_label_create(btn_ok);
    lv_label_set_text(ol, "确认");
    lv_obj_set_style_text_color(ol, COLOR_BG, 0);
    lv_obj_set_style_text_font(ol, FONT_MD, 0);
    lv_obj_center(ol);
}

void buddy_ui_show_reset_confirm(bool show) {
    obj_set_hidden(s_reset_overlay, !show);
    if (show) {
        lv_obj_move_foreground(s_reset_overlay);
    }
}

bool buddy_ui_is_reset_confirm_visible(void) {
    return s_reset_overlay && !lv_obj_has_flag(s_reset_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* ============ 回调注册 ============ */

void buddy_ui_set_overlay_close_cb(BuddyOverlayCloseCb menu_cb,
                                    BuddyOverlayCloseCb settings_cb) {
    s_menu_close_cb = menu_cb;
    s_settings_close_cb = settings_cb;
}

void buddy_ui_set_menu_action_cb(BuddyMenuActionCb cb) {
    s_menu_action_cb = cb;
}

void buddy_ui_set_settings_action_cb(BuddySettingActionCb cb) {
    s_settings_action_cb = cb;
}

void buddy_ui_set_reset_confirm_cb(BuddyResetConfirmCb cb) {
    s_reset_confirm_cb = cb;
}

/* ============ 初始化 ============ */

void buddy_ui_init(void) {
    if (s_container) return;

    s_main_scr = lv_scr_act();

    /* 主容器 */
    s_container = lv_obj_create(s_main_scr);
    lv_obj_set_size(s_container, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_container, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_container, 0, 0);
    lv_obj_set_style_radius(s_container, 0, 0);
    obj_set_pad_all(s_container, 0);
    lv_obj_align(s_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_container, LV_OBJ_FLAG_HIDDEN);

    /* 创建各页面 */
    create_normal_page();
    create_info_page();
    create_menu_overlay();
    create_settings_overlay();
    create_reset_overlay();

    /* 注册驱动层滑动检测回调 */
    lvgl_touch_set_swipe_cb(buddy_ui_handle_swipe);

    /* 初始化显示 */
    switch_mode_pages();
    update_info_content();
    buddy_ui_set_clock(12, 0, 0);

    buddy_drawer_init(s_container);

    ESP_LOGI(TAG, "Buddy UI initialized");
}

void buddy_ui_show(bool show) {
    ESP_LOGI(TAG, "buddy_ui_show: show=%d, container=%p", show, s_container);
    s_ui_visible = show;
    obj_set_hidden(s_container, !show);
    if (show) {
        lv_obj_move_foreground(s_container);
        ESP_LOGI(TAG, "Buddy UI shown, parent=%p, hidden=%d", 
                 s_container ? lv_obj_get_parent(s_container) : NULL,
                 s_container ? lv_obj_has_flag(s_container, LV_OBJ_FLAG_HIDDEN) : -1);
    }
}

bool buddy_ui_is_visible(void) {
    return s_ui_visible;
}

lv_obj_t *buddy_ui_get_container(void) {
    return s_container;
}

/* ============ 模式切换 ============ */

void buddy_ui_set_mode(BuddyMode mode) {
    if (mode < 0 || mode >= BUDDY_MODE_MAX) return;
    s_mode = mode;
    switch_mode_pages();
    if (mode == BUDDY_MODE_INFO) {
        update_info_content();
    }
}

BuddyMode buddy_ui_get_mode(void) {
    return s_mode;
}

/* ============ NORMAL 页面 ============ */

void buddy_ui_set_persona(uint8_t persona_state) {
    /* 状态未变化时直接返回，避免每帧 set_text / set_style 触发重绘 */
    if ((int)persona_state == s_last_persona) return;
    s_last_persona = (int)persona_state;

    if (s_persona_label) {
        lv_label_set_text(s_persona_label, buddy_persona_label((PersonaState)persona_state));
        lv_obj_set_style_text_color(s_persona_label,
            persona_state == PERSONA_IDLE ? COLOR_TEXT_DIM : COLOR_TEXT, 0);
    }
    if (s_mood_dots[1]) {
        bool active = persona_state != PERSONA_IDLE;
        lv_obj_set_style_bg_color(s_mood_dots[1], active ? COLOR_ACCENT : COLOR_LINE, 0);
    }
}


/* ============ PET 页面 ============ */

/* ============ INFO 页面 ============ */

void buddy_ui_set_info_page(InfoPageIdx page) {
    if (page < 0 || page >= INFO_PAGE_MAX) return;
    s_info_idx = page;
    update_info_content();
}

InfoPageIdx buddy_ui_get_info_page(void) {
    return s_info_idx;
}

/* ============ 菜单 ============ */

void buddy_ui_show_menu(bool show) {
    s_menu_visible = show;
    obj_set_hidden(s_menu_overlay, !show);
    if (show) {
        lv_obj_move_foreground(s_menu_overlay);
        /* 重置选中 */
        s_menu_selected = 0;
        for (int i = 0; i < BUDDY_MENU_MAX; i++) {
            apply_row_selection(s_menu_items[i], s_menu_bars[i], s_menu_labels[i], i == 0, false);
        }
    }
}

bool buddy_ui_is_menu_visible(void) {
    return s_menu_visible;
}

void buddy_ui_menu_select(BuddyMenuItem item) {
    if (item < 0 || item >= BUDDY_MENU_MAX) return;
    s_menu_selected = item;
    for (int i = 0; i < BUDDY_MENU_MAX; i++) {
        apply_row_selection(s_menu_items[i], s_menu_bars[i], s_menu_labels[i], i == item, false);
    }
}

BuddyMenuItem buddy_ui_get_menu_selected(void) {
    return (BuddyMenuItem)s_menu_selected;
}

/* ============ 设置 ============ */

void buddy_ui_show_settings(bool show) {
    s_settings_visible = show;
    obj_set_hidden(s_settings_overlay, !show);
    if (show) {
        lv_obj_move_foreground(s_settings_overlay);
        for (int i = 0; i < BUDDY_SET_MAX; i++) {
            apply_row_selection(s_setting_items[i], s_setting_bars[i], s_setting_labels[i], i == 0, false);
        }
    }
}

bool buddy_ui_is_settings_visible(void) {
    return s_settings_visible;
}

void buddy_ui_settings_select(BuddySettingItem item) {
    if (item < 0 || item >= BUDDY_SET_MAX) return;
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        apply_row_selection(s_setting_items[i], s_setting_bars[i], s_setting_labels[i], i == item, false);
    }
}

void buddy_ui_settings_set_toggle(BuddySettingItem item, bool on) {
    if (item < 0 || item >= BUDDY_SET_MAX) return;
    switch (item) {
    case BUDDY_SET_INFO_PANEL: s_set_info_panel = on; break;
    case BUDDY_SET_ROTATE:     s_set_rotate = on; break;
    case BUDDY_SET_WIFI:       s_set_wifi = on;   break;
    case BUDDY_SET_SOUND:      s_set_sound = on;  break;
    case BUDDY_SET_AUTO_SLEEP: s_set_auto_sleep = on; break;
    default: break;
    }
    if (s_setting_vals[item]) {
        if (item == BUDDY_SET_BRIGHTNESS) {
            /* 亮度由单独函数处理 */
        } else if (item == BUDDY_SET_RESET) {
            lv_label_set_text(s_setting_vals[item], "");
        } else if (item == BUDDY_SET_ROTATE) {
            /* 旋转由 buddy_ui_settings_set_rotate() 显示角度 */
        } else {
            lv_label_set_text(s_setting_vals[item], on ? "开" : "关");
            lv_obj_set_style_text_color(s_setting_vals[item], on ? COLOR_TEXT : COLOR_TEXT_DIM, 0);
        }
    }
}

void buddy_ui_settings_set_brightness(int pct) {
    s_brightness_pct = pct;
    if (s_brightness_pct < 0) s_brightness_pct = 0;
    if (s_brightness_pct > 100) s_brightness_pct = 100;
    if (s_setting_vals[BUDDY_SET_BRIGHTNESS]) {
        lv_label_set_text_fmt(s_setting_vals[BUDDY_SET_BRIGHTNESS], "%d%%", s_brightness_pct);
    }
}

void buddy_ui_settings_set_rotate(uint8_t rot) {
    static const char *deg[] = { "0°", "90°", "180°" };
    if (rot > 2) rot = 0;
    if (s_setting_vals[BUDDY_SET_ROTATE]) {
        lv_label_set_text(s_setting_vals[BUDDY_SET_ROTATE], deg[rot]);
        lv_obj_set_style_text_color(s_setting_vals[BUDDY_SET_ROTATE], COLOR_TEXT_DIM, 0);
    }
}

/* ============ 时钟 ============ */

void buddy_ui_set_clock(int hour, int minute, int second) {
    /* 只在「显示的分钟」变化时才刷新 label：主循环每 100ms 调用一次，
     * 不做判断会每帧重排/重绘时钟标签 */
    bool changed = (hour != s_clock_h) || (minute != s_clock_m);
    s_clock_h = hour;
    s_clock_m = minute;
    s_clock_s = second;
    if (s_clock_label && changed) {
        lv_label_set_text_fmt(s_clock_label, "%02d:%02d", hour, minute);
    }
}

/* ============ 电池状态 ============ */

void buddy_ui_set_battery(int percentage, bool charging) {
    if (!s_battery_label) return;
    const char *symbol = charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL;
    if (percentage < 20) symbol = charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_EMPTY;
    else if (percentage < 50) symbol = charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_1;
    else if (percentage < 75) symbol = charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_2;
    else if (percentage < 90) symbol = charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_3;
    lv_label_set_text_fmt(s_battery_label, "%s %d%%", symbol, percentage);
}

/* ============ WiFi 状态 ============ */

/**
 * 更新顶部 WiFi 连接标识
 * @param enabled   WiFi 功能是否开启（设置中关闭时不显示）
 * @param signal_level 信号强度等级：0=未连接/极弱, 1=弱, 2=中, 3=强
 */
void buddy_ui_set_wifi(bool enabled, int signal_level) {
    if (!s_wifi_label) return;
    /* 开关与信号等级都未变化时直接返回（主循环每帧调用） */
    if ((int)enabled == s_last_wifi_en && signal_level == s_last_wifi_lvl) return;
    s_last_wifi_en  = (int)enabled;
    s_last_wifi_lvl = signal_level;
    if (!enabled) {
        lv_obj_add_flag(s_wifi_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_wifi_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_wifi_label, LV_SYMBOL_WIFI);
    /* 按信号等级着色：强=绿, 中=薄荷青, 弱=红, 未连接/极弱=暗色 */
    lv_color_t color = COLOR_TEXT_DIM;
    if (signal_level >= 3)      color = COLOR_GREEN;
    else if (signal_level == 2) color = COLOR_ACCENT;
    else if (signal_level == 1) color = COLOR_RED;
    lv_obj_set_style_text_color(s_wifi_label, color, 0);

    /* 诊断：信号等级变化时打印一次，确认 UI 实际收到的等级 */
    if (signal_level != s_last_wifi_level) {
        s_last_wifi_level = signal_level;
        ESP_LOGI(TAG, "WiFi indicator level=%d color=0x%04X", signal_level, color.full);
    }
}

/* ============ 动画 tick ============ */

void buddy_ui_anim_tick(uint32_t tick) {
    (void)tick;
    /* 已移除精灵图/图片播放，主页统一使用 ASCII 角色动画 */
    if (s_mode == BUDDY_MODE_NORMAL && s_anim_canvas) {
        buddy_anim_render_to(s_anim_canvas, SCREEN_W / 2, ANIM_AREA_H / 2);
    }
}
