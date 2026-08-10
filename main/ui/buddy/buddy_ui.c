/**
 * @file buddy_ui.c
 * Buddy 主页面 UI 实现
 * 从 claude-desktop-buddy 的 main.cpp 迁移
 */

#include "buddy_ui.h"
#include "buddy_anim.h"
#include "esp_log.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BUDDY_UI";

/* ============ 颜色定义 ============ */
#define COLOR_BG       lv_color_hex(0x0A0A1A)  /* 深蓝黑背景 */
#define COLOR_PANEL    lv_color_hex(0x2104)
#define COLOR_HOT      lv_color_hex(0xFA20)
#define COLOR_GREEN    lv_color_hex(0x07E0)
#define COLOR_RED      lv_color_hex(0xF800)
#define COLOR_TEXT     lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM lv_color_hex(0xA0A0AA)

/* ============ 屏幕尺寸 ============ */
#define SCREEN_W 170
#define SCREEN_H 320

/* ============ 布局常量 ============ */
#define ANIM_AREA_H     170     /* 角色动画区域高度 */
#define STATUS_AREA_H   150     /* 状态区域高度 */
#define STATUS_AREA_Y   170     /* 状态区域 Y 偏移 */
#define BAR_H           8       /* 进度条高度 */
#define ROW_H           28      /* 列表行高 */
#define PAD             4       /* 通用内边距 */

/* ============ 静态 UI 元素 ============ */
static lv_obj_t *s_main_scr = NULL;
static lv_obj_t *s_container = NULL;

/* NORMAL 模式 */
static lv_obj_t *s_normal_page = NULL;
static lv_obj_t *s_anim_canvas = NULL;
static lv_obj_t *s_clock_label = NULL;
static lv_obj_t *s_hud_panel = NULL;
static lv_obj_t *s_hud_label = NULL;
static lv_obj_t *s_battery_label = NULL;

/* PET 模式 */
static lv_obj_t *s_pet_page = NULL;
static lv_obj_t *s_pet_mood_label = NULL;
static lv_obj_t *s_pet_mood_bar = NULL;
static lv_obj_t *s_pet_fed_label = NULL;
static lv_obj_t *s_pet_fed_bar = NULL;
static lv_obj_t *s_pet_energy_label = NULL;
static lv_obj_t *s_pet_energy_bar = NULL;
static lv_obj_t *s_pet_level_label = NULL;

/* INFO 模式 */
static lv_obj_t *s_info_page = NULL;
static lv_obj_t *s_info_title = NULL;
static lv_obj_t *s_info_page_idx = NULL;
static lv_obj_t *s_info_content = NULL;

/* 菜单覆盖层 */
static lv_obj_t *s_menu_overlay = NULL;
static lv_obj_t *s_menu_panel = NULL;
static lv_obj_t *s_menu_items[BUDDY_MENU_MAX];
static lv_obj_t *s_menu_labels[BUDDY_MENU_MAX];

/* 设置覆盖层 */
static lv_obj_t *s_settings_overlay = NULL;
static lv_obj_t *s_settings_panel = NULL;
static lv_obj_t *s_setting_items[BUDDY_SET_MAX];
static lv_obj_t *s_setting_labels[BUDDY_SET_MAX];
static lv_obj_t *s_setting_vals[BUDDY_SET_MAX];

/* 审批覆盖层 */
static lv_obj_t *s_approval_overlay = NULL;
static lv_obj_t *s_approval_panel = NULL;
static lv_obj_t *s_approval_title = NULL;
static lv_obj_t *s_approval_tool = NULL;
static lv_obj_t *s_approval_prompt = NULL;
static lv_obj_t *s_btn_approve = NULL;
static lv_obj_t *s_btn_reject = NULL;
static lv_obj_t *s_lbl_approve = NULL;
static lv_obj_t *s_lbl_reject = NULL;

/* ============ 状态变量 ============ */
static BuddyMode s_mode = BUDDY_MODE_NORMAL;
static bool s_ui_visible = false;
static bool s_menu_visible = false;
static bool s_settings_visible = false;
static bool s_approval_visible = false;
static InfoPageIdx s_info_idx = INFO_PAGE_ABOUT;
static int s_menu_selected = 0;
static int s_settings_selected = 0;

/* 时钟 */
static int s_clock_h = 12;
static int s_clock_m = 0;
static int s_clock_s = 0;

/* PET 统计 */
static int s_pet_mood = 50;
static int s_pet_fed = 50;
static int s_pet_energy = 50;
static int s_pet_level = 1;

/* 设置状态 */
static int  s_brightness_pct = 80;
static bool s_set_sound = true;
static bool s_set_wifi = false;
static bool s_set_led = true;
static bool s_set_hud = true;
static bool s_set_rotate = false;
static bool s_set_ascii = false;
static bool s_set_auto_sleep = false;

/* 外部关闭回调 */
static BuddyOverlayCloseCb s_menu_close_cb = NULL;
static BuddyOverlayCloseCb s_settings_close_cb = NULL;
static BuddyOverlayCloseCb s_approval_close_cb = NULL;

/* 前向声明 */
static void info_gesture_cb(lv_event_t *e);

/* ============ 菜单文本 ============ */
static const char *s_menu_texts[BUDDY_MENU_MAX] = {
    LV_SYMBOL_SETTINGS " Settings",
    LV_SYMBOL_POWER  " Shutdown",
    LV_SYMBOL_WARNING  " Help",
    LV_SYMBOL_HOME   " About",
    LV_SYMBOL_PLAY   " Demo",
    LV_SYMBOL_CLOSE  " Close",
};

/* ============ 设置项文本 ============ */
static const char *s_setting_texts[BUDDY_SET_MAX] = {
    LV_SYMBOL_IMAGE     " Brightness",
    LV_SYMBOL_VOLUME_MAX" Sound",
    LV_SYMBOL_WIFI      " Wi-Fi",
    LV_SYMBOL_BULLET    " LED",
    LV_SYMBOL_EYE_OPEN  " HUD",
    LV_SYMBOL_REFRESH   " Rotate",
    LV_SYMBOL_FILE      " ASCII Pet",
    LV_SYMBOL_CHARGE    " Auto Sleep",
    LV_SYMBOL_TRASH     " Reset",
    LV_SYMBOL_LEFT      " Back",
};

/* ============ Info 页面标题 ============ */
static const char *s_info_titles[INFO_PAGE_MAX] = {
    "About",
    "Buttons",
    "Claude",
    "Device",
    "Network",
    "Battery",
    "Credits",
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
    obj_set_hidden(s_pet_page,    s_mode != BUDDY_MODE_PET);
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
    lv_obj_set_style_text_font(s_clock_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_LEFT, 4, 4);

    /* 电池标签（右上角） */
    s_battery_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_battery_label, LV_SYMBOL_CHARGE " 100%");
    lv_obj_set_style_text_color(s_battery_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_battery_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* HUD 面板 */
    s_hud_panel = lv_obj_create(s_normal_page);
    lv_obj_set_size(s_hud_panel, SCREEN_W - 8, 60);
    lv_obj_set_style_bg_color(s_hud_panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(s_hud_panel, 1, 0);
    lv_obj_set_style_border_color(s_hud_panel, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(s_hud_panel, 6, 0);
    obj_set_pad_all(s_hud_panel, 4);
    lv_obj_align(s_hud_panel, LV_ALIGN_TOP_MID, 0, STATUS_AREA_Y + 10);
    lv_obj_clear_flag(s_hud_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_hud_panel, LV_OBJ_FLAG_HIDDEN);

    s_hud_label = lv_label_create(s_hud_panel);
    lv_label_set_text(s_hud_label, "HUD Ready");
    lv_obj_set_style_text_color(s_hud_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_hud_label, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_hud_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_hud_label, SCREEN_W - 20);
    lv_obj_align(s_hud_label, LV_ALIGN_TOP_LEFT, 0, 0);
}

/* ============ PET 页面创建 ============ */

static void create_pet_page(void) {
    s_pet_page = lv_obj_create(s_container);
    lv_obj_set_size(s_pet_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_pet_page, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_pet_page, 0, 0);
    lv_obj_set_style_radius(s_pet_page, 0, 0);
    obj_set_pad_all(s_pet_page, 0);
    lv_obj_align(s_pet_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_pet_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pet_page, LV_OBJ_FLAG_HIDDEN);

    /* 标题 */
    lv_obj_t *title = lv_label_create(s_pet_page);
    lv_label_set_text(title, LV_SYMBOL_HOME " PET STATUS");
    lv_obj_set_style_text_color(title, COLOR_HOT, 0);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    int y = 45;

    /* Mood */
    s_pet_mood_label = lv_label_create(s_pet_page);
    lv_label_set_text(s_pet_mood_label, "Mood");
    lv_obj_set_style_text_color(s_pet_mood_label, COLOR_TEXT, 0);
    lv_obj_align(s_pet_mood_label, LV_ALIGN_TOP_LEFT, 8, y);

    s_pet_mood_bar = lv_bar_create(s_pet_page);
    lv_obj_set_size(s_pet_mood_bar, SCREEN_W - 60, BAR_H);
    lv_bar_set_range(s_pet_mood_bar, 0, 100);
    lv_bar_set_value(s_pet_mood_bar, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_pet_mood_bar, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_pet_mood_bar, COLOR_HOT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_pet_mood_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_pet_mood_bar, 4, LV_PART_INDICATOR);
    lv_obj_align(s_pet_mood_bar, LV_ALIGN_TOP_RIGHT, -8, y + 4);
    y += ROW_H;

    /* Fed */
    s_pet_fed_label = lv_label_create(s_pet_page);
    lv_label_set_text(s_pet_fed_label, "Fed");
    lv_obj_set_style_text_color(s_pet_fed_label, COLOR_TEXT, 0);
    lv_obj_align(s_pet_fed_label, LV_ALIGN_TOP_LEFT, 8, y);

    s_pet_fed_bar = lv_bar_create(s_pet_page);
    lv_obj_set_size(s_pet_fed_bar, SCREEN_W - 60, BAR_H);
    lv_bar_set_range(s_pet_fed_bar, 0, 100);
    lv_bar_set_value(s_pet_fed_bar, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_pet_fed_bar, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_pet_fed_bar, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_pet_fed_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_pet_fed_bar, 4, LV_PART_INDICATOR);
    lv_obj_align(s_pet_fed_bar, LV_ALIGN_TOP_RIGHT, -8, y + 4);
    y += ROW_H;

    /* Energy */
    s_pet_energy_label = lv_label_create(s_pet_page);
    lv_label_set_text(s_pet_energy_label, "Energy");
    lv_obj_set_style_text_color(s_pet_energy_label, COLOR_TEXT, 0);
    lv_obj_align(s_pet_energy_label, LV_ALIGN_TOP_LEFT, 8, y);

    s_pet_energy_bar = lv_bar_create(s_pet_page);
    lv_obj_set_size(s_pet_energy_bar, SCREEN_W - 60, BAR_H);
    lv_bar_set_range(s_pet_energy_bar, 0, 100);
    lv_bar_set_value(s_pet_energy_bar, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_pet_energy_bar, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_pet_energy_bar, lv_color_hex(0x64B4FF), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_pet_energy_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_pet_energy_bar, 4, LV_PART_INDICATOR);
    lv_obj_align(s_pet_energy_bar, LV_ALIGN_TOP_RIGHT, -8, y + 4);
    y += ROW_H;

    /* Level */
    lv_obj_t *lvl_label = lv_label_create(s_pet_page);
    lv_label_set_text(lvl_label, "Level");
    lv_obj_set_style_text_color(lvl_label, COLOR_TEXT, 0);
    lv_obj_align(lvl_label, LV_ALIGN_TOP_LEFT, 8, y);

    s_pet_level_label = lv_label_create(s_pet_page);
    lv_label_set_text(s_pet_level_label, "1");
    lv_obj_set_style_text_color(s_pet_level_label, COLOR_HOT, 0);
    lv_obj_align(s_pet_level_label, LV_ALIGN_TOP_RIGHT, -8, y);
    y += ROW_H;

    /* ASCII Pet 预览区 */
    lv_obj_t *ascii_box = lv_obj_create(s_pet_page);
    lv_obj_set_size(ascii_box, SCREEN_W - 16, 90);
    lv_obj_set_style_bg_color(ascii_box, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(ascii_box, 1, 0);
    lv_obj_set_style_border_color(ascii_box, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(ascii_box, 6, 0);
    obj_set_pad_all(ascii_box, 4);
    lv_obj_align(ascii_box, LV_ALIGN_TOP_MID, 0, y + 8);
    lv_obj_clear_flag(ascii_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ascii_title = lv_label_create(ascii_box);
    lv_label_set_text(ascii_title, "~ ascii pet ~");
    lv_obj_set_style_text_color(ascii_title, COLOR_TEXT_DIM, 0);
    lv_obj_align(ascii_title, LV_ALIGN_TOP_MID, 0, 0);

    /* 底部提示 */
    lv_obj_t *hint = lv_label_create(s_pet_page);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " swipe to exit " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/* ============ INFO 页面创建 ============ */

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
    lv_obj_add_flag(s_info_page, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_info_page, info_gesture_cb, LV_EVENT_GESTURE, NULL);

    /* 标题栏 */
    lv_obj_t *title_bar = lv_obj_create(s_info_page);
    lv_obj_set_size(title_bar, SCREEN_W, 32);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    obj_set_pad_all(title_bar, 4);
    lv_obj_align(title_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_info_title = lv_label_create(title_bar);
    lv_label_set_text(s_info_title, "About");
    lv_obj_set_style_text_color(s_info_title, COLOR_TEXT, 0);
    lv_obj_align(s_info_title, LV_ALIGN_LEFT_MID, 4, 0);

    s_info_page_idx = lv_label_create(title_bar);
    lv_label_set_text(s_info_page_idx, "1/6");
    lv_obj_set_style_text_color(s_info_page_idx, COLOR_TEXT_DIM, 0);
    lv_obj_align(s_info_page_idx, LV_ALIGN_RIGHT_MID, -4, 0);

    /* 内容区域 */
    s_info_content = lv_label_create(s_info_page);
    lv_obj_set_style_text_color(s_info_content, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_info_content, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_info_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(s_info_content, SCREEN_W - 12, SCREEN_H - 56);
    lv_obj_align(s_info_content, LV_ALIGN_TOP_LEFT, 6, 38);

    /* 底部导航提示 */
    lv_obj_t *hint = lv_label_create(s_info_page);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " prev        next " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
}

static void update_info_content(void) {
    if (!s_info_content) return;

    lv_label_set_text_fmt(s_info_title, "%s", s_info_titles[s_info_idx]);
    lv_label_set_text_fmt(s_info_page_idx, "%d/%d", s_info_idx + 1, INFO_PAGE_MAX);

    switch (s_info_idx) {
    case INFO_PAGE_ABOUT: {
        lv_label_set_text(s_info_content,
            "Elara Buddy\n"
            "===========\n"
            "A desktop companion\n"
            "for ESP32-S3\n\n"
            "Version: 1.0.0\n"
            "LVGL: 9.1\n"
            "ESP-IDF: 5.5.4\n\n"
            "Made with love\n"
            "and caffeine.");
        break;
    }
    case INFO_PAGE_BUTTONS: {
        lv_label_set_text(s_info_content,
            "Button Map\n"
            "==========\n"
            "BOOT: Menu / Back\n"
            "Touch: Navigate\n"
            "Long BOOT: Power\n\n"
            "In Approval:\n"
            "  A = Approve\n"
            "  B = Reject\n\n"
            "In Menu:\n"
            "  Up/Down = Select\n"
            "  Click = Confirm");
        break;
    }
    case INFO_PAGE_CLAUDE: {
        lv_label_set_text(s_info_content,
            "Claude Status\n"
            "=============\n"
            "State: Online\n"
            "Model: claude-sonnet\n"
            "API: Active\n"
            "Latency: ~120ms\n\n"
            "MCP Tools:\n"
            "  file_system\n"
            "  web_search\n"
            "  code_exec\n\n"
            "Status: All OK");
        break;
    }
    case INFO_PAGE_DEVICE: {
        lv_label_set_text(s_info_content,
            "Device Info\n"
            "===========\n"
            "Chip: ESP32-S3\n"
            "CPU: 240MHz\n"
            "Flash: 16MB\n"
            "PSRAM: 8MB\n\n"
            "Display:\n"
            "  170x320 RGB565\n"
            "  ST7789 Driver\n\n"
            "Touch: CST816T\n"
            "I2C: 0x15\n"
            "Battery: ADC GPIO4");
        break;
    }
    case INFO_PAGE_NETWORK: {
        extern bool wifi_manager_get_ip(char *ip_str, size_t max_len);
        char ip[16] = "-";
        wifi_manager_get_ip(ip, sizeof(ip));
        lv_label_set_text_fmt(s_info_content,
            "Network\n"
            "=========\n"
            "WiFi: %s\n"
            "IP: %s\n\n"
            "TCP Server:\n"
            "  Port 8080\n\n"
            "Transport:\n"
            "  JSON line protocol",
            ip[0] ? "Connected" : "Disconnected", ip);
        break;
    }
    case INFO_PAGE_BATTERY: {
        extern uint32_t battery_get_voltage_mv(void);
        extern uint8_t battery_get_percentage(void);
        extern bool battery_is_charging(void);
        lv_label_set_text_fmt(s_info_content,
            "Battery\n"
            "=======\n"
            "Status: %s\n"
            "Voltage: %lu mV\n"
            "Level: %d%%\n\n"
            "Health:\n"
            "  Good\n\n"
            "Type:\n"
            "  Li-Po 3.7V",
            battery_is_charging() ? "Charging" : "Discharging",
            (unsigned long)battery_get_voltage_mv(),
            battery_get_percentage());
        break;
    }
    case INFO_PAGE_CREDITS: {
        lv_label_set_text(s_info_content,
            "Credits\n"
            "=======\n"
            "Design: claude-desktop-buddy\n"
            "Port: Elara Team\n\n"
            "Libraries:\n"
            "  LVGL 9.1\n"
            "  esp_lcd_st7789\n"
            "  FreeRTOS\n\n"
            "Hardware:\n"
            "  ESP32-S3-Touch\n\n"
            "Thank you!");
        break;
    }
    default:
        lv_label_set_text(s_info_content, "Unknown page");
        break;
    }
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
            s_menu_close_cb();
        } else if (overlay == s_settings_overlay && s_settings_close_cb) {
            buddy_ui_show_settings(false);
            s_settings_close_cb();
        } else if (overlay == s_approval_overlay && s_approval_close_cb) {
            buddy_ui_hide_approval();
            s_approval_close_cb();
        }
    }
}

/* ============ INFO 页面滑动手势 ============ */

static void info_gesture_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_RIGHT) {
        buddy_ui_info_prev();
    } else if (dir == LV_DIR_LEFT) {
        buddy_ui_info_next();
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
            /* 高亮选中项 */
            for (int j = 0; j < BUDDY_MENU_MAX; j++) {
                lv_obj_set_style_bg_color(s_menu_items[j], (j == i) ? COLOR_HOT : COLOR_PANEL, 0);
            }
            break;
        }
    }
}

static void create_menu_overlay(void) {
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

    /* 面板 */
    s_menu_panel = lv_obj_create(s_menu_overlay);
    lv_obj_set_size(s_menu_panel, 140, 180);
    lv_obj_set_style_bg_color(s_menu_panel, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_menu_panel, 1, 0);
    lv_obj_set_style_border_color(s_menu_panel, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(s_menu_panel, 8, 0);
    obj_set_pad_all(s_menu_panel, 4);
    lv_obj_center(s_menu_panel);
    lv_obj_clear_flag(s_menu_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 菜单标题 */
    lv_obj_t *mt = lv_label_create(s_menu_panel);
    lv_label_set_text(mt, LV_SYMBOL_LIST " Menu");
    lv_obj_set_style_text_color(mt, COLOR_HOT, 0);
    lv_obj_align(mt, LV_ALIGN_TOP_MID, 0, 2);

    /* 菜单项 */
    int y = 26;
    for (int i = 0; i < BUDDY_MENU_MAX; i++) {
        s_menu_items[i] = lv_obj_create(s_menu_panel);
        lv_obj_set_size(s_menu_items[i], 128, 22);
        lv_obj_set_style_bg_color(s_menu_items[i], (i == 0) ? COLOR_HOT : COLOR_PANEL, 0);
        lv_obj_set_style_border_width(s_menu_items[i], 0, 0);
        lv_obj_set_style_radius(s_menu_items[i], 4, 0);
        obj_set_pad_all(s_menu_items[i], 2);
        lv_obj_align(s_menu_items[i], LV_ALIGN_TOP_MID, 0, y);
        lv_obj_add_flag(s_menu_items[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_menu_items[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(s_menu_items[i], menu_event_cb, LV_EVENT_CLICKED, NULL);

        s_menu_labels[i] = lv_label_create(s_menu_items[i]);
        lv_label_set_text(s_menu_labels[i], s_menu_texts[i]);
        lv_obj_set_style_text_color(s_menu_labels[i], COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_menu_labels[i], LV_FONT_DEFAULT, 0);
        lv_obj_align(s_menu_labels[i], LV_ALIGN_LEFT_MID, 4, 0);

        y += 24;
    }
}

/* ============ 设置覆盖层创建 ============ */

static void settings_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        if (s_setting_items[i] == target) {
            s_settings_selected = i;
            ESP_LOGI(TAG, "Settings clicked: %d", i);
            for (int j = 0; j < BUDDY_SET_MAX; j++) {
                lv_obj_set_style_bg_color(s_setting_items[j], (j == i) ? COLOR_HOT : COLOR_PANEL, 0);
            }
            break;
        }
    }
}

static void create_settings_overlay(void) {
    s_settings_overlay = lv_obj_create(s_container);
    lv_obj_set_size(s_settings_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_settings_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_settings_overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(s_settings_overlay, 0, 0);
    lv_obj_set_style_radius(s_settings_overlay, 0, 0);
    obj_set_pad_all(s_settings_overlay, 0);
    lv_obj_align(s_settings_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_settings_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_settings_overlay, overlay_click_close_cb, LV_EVENT_CLICKED, s_settings_overlay);

    s_settings_panel = lv_obj_create(s_settings_overlay);
    lv_obj_set_size(s_settings_panel, 154, 260);
    lv_obj_set_style_bg_color(s_settings_panel, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_settings_panel, 1, 0);
    lv_obj_set_style_border_color(s_settings_panel, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(s_settings_panel, 8, 0);
    obj_set_pad_all(s_settings_panel, 4);
    lv_obj_center(s_settings_panel);
    lv_obj_clear_flag(s_settings_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 设置标题 */
    lv_obj_t *st = lv_label_create(s_settings_panel);
    lv_label_set_text(st, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_color(st, COLOR_HOT, 0);
    lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 2);

    int y = 24;
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        s_setting_items[i] = lv_obj_create(s_settings_panel);
        lv_obj_set_size(s_setting_items[i], 142, 22);
        lv_obj_set_style_bg_color(s_setting_items[i], (i == 0) ? COLOR_HOT : COLOR_PANEL, 0);
        lv_obj_set_style_border_width(s_setting_items[i], 0, 0);
        lv_obj_set_style_radius(s_setting_items[i], 4, 0);
        obj_set_pad_all(s_setting_items[i], 2);
        lv_obj_align(s_setting_items[i], LV_ALIGN_TOP_MID, 0, y);
        lv_obj_add_flag(s_setting_items[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_setting_items[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(s_setting_items[i], settings_event_cb, LV_EVENT_CLICKED, NULL);

        s_setting_labels[i] = lv_label_create(s_setting_items[i]);
        lv_label_set_text(s_setting_labels[i], s_setting_texts[i]);
        lv_obj_set_style_text_color(s_setting_labels[i], COLOR_TEXT, 0);
        lv_obj_set_style_text_font(s_setting_labels[i], LV_FONT_DEFAULT, 0);
        lv_obj_align(s_setting_labels[i], LV_ALIGN_LEFT_MID, 4, 0);

        s_setting_vals[i] = lv_label_create(s_setting_items[i]);
        lv_obj_set_style_text_color(s_setting_vals[i], COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(s_setting_vals[i], LV_FONT_DEFAULT, 0);
        lv_obj_align(s_setting_vals[i], LV_ALIGN_RIGHT_MID, -4, 0);

        y += 24;
    }

    /* 初始化设置值显示 */
    buddy_ui_settings_set_brightness(s_brightness_pct);
    buddy_ui_settings_set_toggle(BUDDY_SET_SOUND, s_set_sound);
    buddy_ui_settings_set_toggle(BUDDY_SET_WIFI, s_set_wifi);
    buddy_ui_settings_set_toggle(BUDDY_SET_LED, s_set_led);
    buddy_ui_settings_set_toggle(BUDDY_SET_HUD, s_set_hud);
    buddy_ui_settings_set_toggle(BUDDY_SET_ROTATE, s_set_rotate);
    buddy_ui_settings_set_toggle(BUDDY_SET_ASCII, s_set_ascii);
    buddy_ui_settings_set_toggle(BUDDY_SET_AUTO_SLEEP, s_set_auto_sleep);
    lv_label_set_text(s_setting_vals[BUDDY_SET_RESET], "");
    lv_label_set_text(s_setting_vals[BUDDY_SET_BACK], "");
}

/* ============ 审批覆盖层创建 ============ */

static void approval_btn_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    lv_obj_t *target = lv_event_get_target(e);
    if (target == s_btn_approve) {
        ESP_LOGI(TAG, "Approval: APPROVED");
        buddy_ui_hide_approval();
    } else if (target == s_btn_reject) {
        ESP_LOGI(TAG, "Approval: REJECTED");
        buddy_ui_hide_approval();
    }
}

static void create_approval_overlay(void) {
    s_approval_overlay = lv_obj_create(s_container);
    lv_obj_set_size(s_approval_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_approval_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_approval_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_approval_overlay, 0, 0);
    lv_obj_set_style_radius(s_approval_overlay, 0, 0);
    obj_set_pad_all(s_approval_overlay, 0);
    lv_obj_align(s_approval_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_approval_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_approval_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_approval_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_approval_overlay, overlay_click_close_cb, LV_EVENT_CLICKED, s_approval_overlay);

    s_approval_panel = lv_obj_create(s_approval_overlay);
    lv_obj_set_size(s_approval_panel, 156, 200);
    lv_obj_set_style_bg_color(s_approval_panel, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_approval_panel, 2, 0);
    lv_obj_set_style_border_color(s_approval_panel, COLOR_HOT, 0);
    lv_obj_set_style_radius(s_approval_panel, 8, 0);
    obj_set_pad_all(s_approval_panel, 6);
    lv_obj_center(s_approval_panel);
    lv_obj_clear_flag(s_approval_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    s_approval_title = lv_label_create(s_approval_panel);
    lv_label_set_text(s_approval_title, "APPROVAL REQUIRED");
    lv_obj_set_style_text_color(s_approval_title, COLOR_HOT, 0);
    lv_obj_set_style_text_font(s_approval_title, LV_FONT_DEFAULT, 0);
    lv_obj_align(s_approval_title, LV_ALIGN_TOP_MID, 0, 4);

    /* 工具名 */
    s_approval_tool = lv_label_create(s_approval_panel);
    lv_label_set_text(s_approval_tool, "Tool: --");
    lv_obj_set_style_text_color(s_approval_tool, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_approval_tool, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_approval_tool, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_approval_tool, 140);
    lv_obj_align(s_approval_tool, LV_ALIGN_TOP_MID, 0, 26);

    /* 提示文本 */
    s_approval_prompt = lv_label_create(s_approval_panel);
    lv_label_set_text(s_approval_prompt, "No prompt");
    lv_obj_set_style_text_color(s_approval_prompt, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_approval_prompt, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(s_approval_prompt, LV_LABEL_LONG_WRAP);
    lv_obj_set_size(s_approval_prompt, 140, 80);
    lv_obj_align(s_approval_prompt, LV_ALIGN_TOP_MID, 0, 50);

    /* A 键批准按钮 */
    s_btn_approve = lv_btn_create(s_approval_panel);
    lv_obj_set_size(s_btn_approve, 64, 32);
    lv_obj_set_style_bg_color(s_btn_approve, COLOR_GREEN, 0);
    lv_obj_set_style_radius(s_btn_approve, 6, 0);
    lv_obj_align(s_btn_approve, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    lv_obj_add_event_cb(s_btn_approve, approval_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_approve = lv_label_create(s_btn_approve);
    lv_label_set_text(s_lbl_approve, "A OK");
    lv_obj_set_style_text_color(s_lbl_approve, COLOR_TEXT, 0);
    lv_obj_center(s_lbl_approve);

    /* B 键拒绝按钮 */
    s_btn_reject = lv_btn_create(s_approval_panel);
    lv_obj_set_size(s_btn_reject, 64, 32);
    lv_obj_set_style_bg_color(s_btn_reject, COLOR_RED, 0);
    lv_obj_set_style_radius(s_btn_reject, 6, 0);
    lv_obj_align(s_btn_reject, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
    lv_obj_add_event_cb(s_btn_reject, approval_btn_cb, LV_EVENT_CLICKED, NULL);

    s_lbl_reject = lv_label_create(s_btn_reject);
    lv_label_set_text(s_lbl_reject, "B NO");
    lv_obj_set_style_text_color(s_lbl_reject, COLOR_TEXT, 0);
    lv_obj_center(s_lbl_reject);
}

/* ============ 回调注册 ============ */

void buddy_ui_set_overlay_close_cb(BuddyOverlayCloseCb menu_cb,
                                    BuddyOverlayCloseCb settings_cb,
                                    BuddyOverlayCloseCb approval_cb) {
    s_menu_close_cb = menu_cb;
    s_settings_close_cb = settings_cb;
    s_approval_close_cb = approval_cb;
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
    create_pet_page();
    create_info_page();
    create_menu_overlay();
    create_settings_overlay();
    create_approval_overlay();

    /* 初始化显示 */
    switch_mode_pages();
    update_info_content();
    buddy_ui_set_clock(12, 0, 0);

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

void buddy_ui_set_hud_text(const char *text) {
    if (!s_hud_label || !text) return;
    lv_label_set_text(s_hud_label, text);
}

void buddy_ui_set_hud_visible(bool visible) {
    obj_set_hidden(s_hud_panel, !visible);
}

void buddy_ui_show_approval(const char *tool_name, const char *prompt) {
    if (!s_approval_overlay) return;
    if (tool_name) {
        lv_label_set_text_fmt(s_approval_tool, "Tool: %s", tool_name);
    }
    if (prompt) {
        lv_label_set_text(s_approval_prompt, prompt);
    }
    lv_obj_clear_flag(s_approval_overlay, LV_OBJ_FLAG_HIDDEN);
    s_approval_visible = true;
    lv_obj_move_foreground(s_approval_overlay);
}

void buddy_ui_hide_approval(void) {
    obj_set_hidden(s_approval_overlay, true);
    s_approval_visible = false;
}

bool buddy_ui_is_approval_visible(void) {
    return s_approval_visible;
}

/* ============ PET 页面 ============ */

void buddy_ui_set_pet_stats(int mood, int fed, int energy, int level) {
    s_pet_mood = mood;
    s_pet_fed = fed;
    s_pet_energy = energy;
    s_pet_level = level;

    if (s_pet_mood_bar)   lv_bar_set_value(s_pet_mood_bar, mood, LV_ANIM_ON);
    if (s_pet_fed_bar)    lv_bar_set_value(s_pet_fed_bar, fed, LV_ANIM_ON);
    if (s_pet_energy_bar) lv_bar_set_value(s_pet_energy_bar, energy, LV_ANIM_ON);
    if (s_pet_level_label) {
        lv_label_set_text_fmt(s_pet_level_label, "%d", level);
    }
}

/* ============ INFO 页面 ============ */

void buddy_ui_set_info_page(InfoPageIdx page) {
    if (page < 0 || page >= INFO_PAGE_MAX) return;
    s_info_idx = page;
    update_info_content();
}

void buddy_ui_info_next(void) {
    s_info_idx = (s_info_idx + 1) % INFO_PAGE_MAX;
    update_info_content();
}

void buddy_ui_info_prev(void) {
    s_info_idx = (s_info_idx + INFO_PAGE_MAX - 1) % INFO_PAGE_MAX;
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
            lv_obj_set_style_bg_color(s_menu_items[i], (i == 0) ? COLOR_HOT : COLOR_PANEL, 0);
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
        lv_obj_set_style_bg_color(s_menu_items[i], (i == item) ? COLOR_HOT : COLOR_PANEL, 0);
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
        s_settings_selected = 0;
        for (int i = 0; i < BUDDY_SET_MAX; i++) {
            lv_obj_set_style_bg_color(s_setting_items[i], (i == 0) ? COLOR_HOT : COLOR_PANEL, 0);
        }
    }
}

bool buddy_ui_is_settings_visible(void) {
    return s_settings_visible;
}

void buddy_ui_settings_select(BuddySettingItem item) {
    if (item < 0 || item >= BUDDY_SET_MAX) return;
    s_settings_selected = item;
    for (int i = 0; i < BUDDY_SET_MAX; i++) {
        lv_obj_set_style_bg_color(s_setting_items[i], (i == item) ? COLOR_HOT : COLOR_PANEL, 0);
    }
}

BuddySettingItem buddy_ui_get_settings_selected(void) {
    return (BuddySettingItem)s_settings_selected;
}

void buddy_ui_settings_set_toggle(BuddySettingItem item, bool on) {
    if (item < 0 || item >= BUDDY_SET_MAX) return;
    switch (item) {
    case BUDDY_SET_SOUND:  s_set_sound = on;  break;
    case BUDDY_SET_WIFI:   s_set_wifi = on;   break;
    case BUDDY_SET_LED:    s_set_led = on;    break;
    case BUDDY_SET_HUD:    s_set_hud = on;    break;
    case BUDDY_SET_ROTATE: s_set_rotate = on; break;
    case BUDDY_SET_ASCII:  s_set_ascii = on;  break;
    case BUDDY_SET_AUTO_SLEEP: s_set_auto_sleep = on; break;
    default: break;
    }
    if (s_setting_vals[item]) {
        if (item == BUDDY_SET_BRIGHTNESS) {
            /* 亮度由单独函数处理 */
        } else if (item == BUDDY_SET_RESET || item == BUDDY_SET_BACK) {
            lv_label_set_text(s_setting_vals[item], "");
        } else {
            lv_label_set_text(s_setting_vals[item], on ? "ON" : "OFF");
            lv_obj_set_style_text_color(s_setting_vals[item], on ? COLOR_GREEN : COLOR_RED, 0);
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

/* ============ 时钟 ============ */

void buddy_ui_set_clock(int hour, int minute, int second) {
    s_clock_h = hour;
    s_clock_m = minute;
    s_clock_s = second;
    if (s_clock_label) {
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

/* ============ 动画 tick ============ */

void buddy_ui_anim_tick(uint32_t tick) {
    /* 触发 buddy 角色动画 */
    if (s_mode == BUDDY_MODE_NORMAL && s_anim_canvas) {
        /* 使用 buddy_anim 渲染到动画区域 */
        buddy_anim_render_to(s_anim_canvas, SCREEN_W / 2, ANIM_AREA_H / 2);
    }
}
