/**
 * @file wifi_ui.c
 * WiFi 管理页面实现
 *
 * 布局（170×320 纵向）:
 *   ┌────────────────────┐
 *   │ ←   WiFi        ⟳  │  标题栏：返回 / 重扫描
 *   ├────────────────────┤
 *   │      状态行         │  扫描中 / 连接中 / 已连接 / 失败原因
 *   ├────────────────────┤
 *   │ ┌────────────────┐ │
 *   │ │ ● MyWiFi   ▂▄▆█│ │  当前连接（绿色边框置顶）
 *   │ │   Connected IP… │ │
 *   │ ├────────────────┤ │
 *   │ │ 🔒 HomeWiFi ▂▄▆ │ │  已保存（亮面板）
 *   │ │ 🔓 Public   ▂▄  │ │  开放网络（无锁标）
 *   └────────────────────┘  可滚动列表
 *   └────────────────────┘
 *
 * 密码输入采用「全页输入视图」：打开时隐藏列表，自身不透明背景 +
 * 底部固定软键盘，避免小屏上列表文字透过半透明遮罩与键盘叠加显示。
 *
 * 线程模型：所有控件操作均在 LVGL 任务上下文（本模块回调 / 定时器）。
 * wifi_manager 状态变化通过 250ms 轮询版本号感知，无需跨线程调用。
 */

#include "wifi_ui.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#include "lvgl.h"

static const char *TAG = "WIFI_UI";

/* ============ 颜色（与 buddy_ui 薄荷青方案一致） ============ */
#define COLOR_BG       lv_color_hex(0x08141E)
#define COLOR_PANEL    lv_color_hex(0x0F2A38)
#define COLOR_PANEL_LT lv_color_hex(0x12313F)
#define COLOR_HOT      lv_color_hex(0x00D4C8)
#define COLOR_GREEN    lv_color_hex(0x00FF00)
#define COLOR_RED      lv_color_hex(0xFF4040)
#define COLOR_TEXT     lv_color_hex(0xFFFFFF)
#define COLOR_TEXT_DIM lv_color_hex(0x6E92A8)
#define COLOR_CONN_BG  lv_color_hex(0x0D2E24)
#define COLOR_BAR_OFF  lv_color_hex(0x24435A)

/* ============ 布局常量 ============ */
#define SCREEN_W   170
#define SCREEN_H   320
#define HEADER_H   26
#define STATUS_H   16
#define LIST_Y     (HEADER_H + STATUS_H + 2)
#define ITEM_H     46
#define ITEM_GAP   4

/* ============ 页面对象 ============ */
static lv_obj_t *s_page = NULL;        /* 全屏页面（挂在 lv_scr_act） */
static lv_obj_t *s_btn_back = NULL;
static lv_obj_t *s_btn_rescan = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_list = NULL;        /* 可滚动列表容器 */

/* ============ 密码输入（全页视图） ============ */
static lv_obj_t *s_pwd_view = NULL;       /* 全屏输入视图（含键盘，不透明） */
static lv_obj_t *s_pwd_ssid_label = NULL;
static lv_obj_t *s_pwd_ta = NULL;
static lv_obj_t *s_pwd_kb = NULL;
static lv_obj_t *s_pwd_btn_connect = NULL;
static lv_obj_t *s_btn_eye = NULL;
static lv_obj_t *s_pwd_hint = NULL;
static bool s_pwd_show = false;        /* 密码明文显示开关 */
static char s_pwd_ssid[33] = "";       /* 待连接的 SSID */

/* ============ 简化软键盘：数字行常驻 + QWERTY 字母，Shift 切换大小写 ============ */
static const char *s_kb_map_lower[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_BACKSPACE, "\n",
    LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_OK, ""
};
static const char *s_kb_map_upper[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
    LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_OK, ""
};
#define KB_BTN_COUNT 39   /* 10+10+10+9 个按键 */
#define KB_SHIFT_IDX  30  /* 第 4 行第 1 键（Shift） */
static bool s_kb_upper = false;                    /* 当前大写状态 */
static lv_btnmatrix_ctrl_t s_kb_ctrl[KB_BTN_COUNT];

/* ============ 状态 ============ */
static bool s_visible = false;
static lv_timer_t *s_poll_timer = NULL;
static uint32_t s_last_version = 0;
static wifi_item_t s_items[WIFI_MANAGER_MAX_ITEMS];
static size_t s_item_count = 0;

/* 前向声明 */
void wifi_ui_refresh_now(void);

/* ============ 辅助 ============ */

static void obj_set_pad_all(lv_obj_t *obj, lv_coord_t pad) {
    lv_obj_set_style_pad_left(obj, pad, 0);
    lv_obj_set_style_pad_right(obj, pad, 0);
    lv_obj_set_style_pad_top(obj, pad, 0);
    lv_obj_set_style_pad_bottom(obj, pad, 0);
}

static void style_btn(lv_obj_t *btn) {
    lv_obj_set_style_bg_color(btn, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
}

static int rssi_to_bars(int8_t rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    if (rssi >= -80) return 1;
    return 0;
}

static const char *auth_to_str(wifi_auth_mode_t auth) {
    switch (auth) {
    case WIFI_AUTH_OPEN:          return "Open";
    case WIFI_AUTH_WEP:           return "WEP";
    case WIFI_AUTH_WPA_PSK:       return "WPA";
    case WIFI_AUTH_WPA2_PSK:      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default:                      return "Secured";
    }
}

static bool is_open_net(wifi_auth_mode_t auth) {
    return auth == WIFI_AUTH_OPEN;
}

/* ============ 信号强度条（4 根高度递增的竖条） ============ */

static void create_signal_bars(lv_obj_t *parent, int bars) {
    static const lv_coord_t heights[4] = {5, 8, 11, 14};
    lv_coord_t x = -32;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *bar = lv_obj_create(parent);
        lv_obj_set_size(bar, 3, heights[i]);
        lv_obj_set_style_bg_color(bar, (i < bars) ? COLOR_GREEN : COLOR_BAR_OFF, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        obj_set_pad_all(bar, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_RIGHT, x, -8);
        x += 5;
    }
}

/* ============ 加密标记（自绘小锁：锁体 + 锁梁） ============ */

static void create_lock_icon(lv_obj_t *parent) {
    lv_obj_t *body = lv_obj_create(parent);
    lv_obj_set_size(body, 8, 6);
    lv_obj_set_style_bg_color(body, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 1, 0);
    obj_set_pad_all(body, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 8, 20);

    lv_obj_t *shackle = lv_obj_create(parent);
    lv_obj_set_size(shackle, 5, 5);
    lv_obj_set_style_bg_opa(shackle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(shackle, 1, 0);
    lv_obj_set_style_border_color(shackle, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_radius(shackle, 2, 0);
    obj_set_pad_all(shackle, 0);
    lv_obj_clear_flag(shackle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(shackle, LV_ALIGN_TOP_LEFT, 10, 16);
}

/* ============ 密码输入弹窗 ============ */

static void pwd_dialog_close(void) {
    if (!s_pwd_view) return;
    s_pwd_show = false;
    lv_obj_add_flag(s_pwd_view, LV_OBJ_FLAG_HIDDEN);
    /* 恢复列表显示 */
    if (s_list) {
        lv_obj_clear_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    }
}

static void pwd_do_connect(void) {
    const char *txt = lv_textarea_get_text(s_pwd_ta);
    size_t len = strlen(txt);
    if (len < 8) {
        lv_label_set_text(s_pwd_hint, "Password needs 8+ chars");
        lv_obj_set_style_text_color(s_pwd_hint, COLOR_RED, 0);
        return;
    }
    char ssid[33];
    strncpy(ssid, s_pwd_ssid, sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    pwd_dialog_close();
    esp_err_t err = wifi_manager_connect(ssid, txt);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Connect failed to start: %s", esp_err_to_name(err));
    }
    wifi_ui_refresh_now();
}

static void pwd_eye_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    s_pwd_show = !s_pwd_show;
    lv_textarea_set_password_mode(s_pwd_ta, !s_pwd_show);
    lv_obj_t *eye = lv_event_get_target(e);
    lv_label_set_text_fmt(lv_obj_get_child(eye, 0), "%s",
                          s_pwd_show ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
}

static void pwd_btn_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    intptr_t which = (intptr_t)lv_event_get_user_data(e);
    if (which == 0) {
        pwd_dialog_close();          /* 取消 */
    } else {
        pwd_do_connect();            /* 连接 */
    }
}

/* 应用大小写状态：切换按键映射并高亮 Shift 键 */
static void kb_apply_shift(lv_obj_t *btnm) {
    memset(s_kb_ctrl, 0, sizeof(s_kb_ctrl));
    if (s_kb_upper) {
        s_kb_ctrl[KB_SHIFT_IDX] = LV_BTNMATRIX_CTRL_CHECKED;
    }
    lv_btnmatrix_set_map(btnm, s_kb_upper ? s_kb_map_upper : s_kb_map_lower);
    lv_btnmatrix_set_ctrl_map(btnm, s_kb_ctrl);
}

static void pwd_kb_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *btnm = lv_event_get_target(e);
    const char *txt = lv_btnmatrix_get_btn_text(
        btnm, lv_btnmatrix_get_selected_btn(btnm));
    if (!txt) return;

    if (strcmp(txt, LV_SYMBOL_UP) == 0) {
        s_kb_upper = !s_kb_upper;              /* Shift：切换大小写 */
        kb_apply_shift(btnm);
    } else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_del_char(s_pwd_ta);        /* 长按连续删除 */
    } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
        pwd_do_connect();                      /* 回车 = 连接 */
    } else {
        lv_textarea_add_text(s_pwd_ta, txt);
    }
}

static void pwd_ta_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED) return;
    /* 输入为空时禁止提交（按钮置灰） */
    const char *txt = lv_textarea_get_text(s_pwd_ta);
    lv_obj_set_style_bg_opa(s_pwd_btn_connect,
                            (txt && txt[0]) ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void create_pwd_dialog(void) {
    /* 全页输入视图：打开时隐藏 WiFi 列表，自身不透明背景，
     * 从根本上避免列表文字透过遮罩与键盘叠加显示 */
    s_pwd_view = lv_obj_create(s_page);
    lv_obj_set_size(s_pwd_view, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_pwd_view, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_pwd_view, 0, 0);
    lv_obj_set_style_radius(s_pwd_view, 0, 0);
    obj_set_pad_all(s_pwd_view, 0);
    lv_obj_align(s_pwd_view, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_pwd_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pwd_view, LV_OBJ_FLAG_HIDDEN);

    /* 取消按钮 + 目标 SSID */
    lv_obj_t *btn_cancel = lv_btn_create(s_pwd_view);
    lv_obj_set_size(btn_cancel, 28, 24);
    lv_obj_set_style_bg_color(btn_cancel, COLOR_PANEL, 0);
    lv_obj_set_style_radius(btn_cancel, 5, 0);
    lv_obj_align(btn_cancel, LV_ALIGN_TOP_LEFT, 2, 4);
    lv_obj_add_event_cb(btn_cancel, pwd_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_t *lbl = lv_label_create(btn_cancel);
    lv_label_set_text(lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    s_pwd_ssid_label = lv_label_create(s_pwd_view);
    lv_label_set_text(s_pwd_ssid_label, "-");
    lv_obj_set_style_text_color(s_pwd_ssid_label, COLOR_HOT, 0);
    lv_label_set_long_mode(s_pwd_ssid_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_pwd_ssid_label, SCREEN_W - 38);
    lv_obj_align(s_pwd_ssid_label, LV_ALIGN_TOP_LEFT, 36, 9);

    /* 密码输入框 */
    s_pwd_ta = lv_textarea_create(s_pwd_view);
    lv_obj_set_size(s_pwd_ta, SCREEN_W - 4, 34);
    lv_textarea_set_one_line(s_pwd_ta, true);
    lv_textarea_set_password_mode(s_pwd_ta, true);
    lv_textarea_set_max_length(s_pwd_ta, 64);   /* ESP32 WiFi 密码上限，防溢出 */
    lv_textarea_set_placeholder_text(s_pwd_ta, "Password (8-64)");
    lv_obj_set_style_bg_color(s_pwd_ta, COLOR_PANEL, 0);
    lv_obj_set_style_text_color(s_pwd_ta, COLOR_TEXT, 0);
    lv_obj_set_style_border_color(s_pwd_ta, COLOR_TEXT_DIM, 0);
    lv_obj_align(s_pwd_ta, LV_ALIGN_TOP_LEFT, 2, 34);
    lv_obj_add_event_cb(s_pwd_ta, pwd_ta_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* [连接] [显示密码] */
    s_pwd_btn_connect = lv_btn_create(s_pwd_view);
    lv_obj_set_size(s_pwd_btn_connect, 76, 28);
    lv_obj_set_style_bg_color(s_pwd_btn_connect, COLOR_HOT, 0);
    lv_obj_set_style_radius(s_pwd_btn_connect, 5, 0);
    lv_obj_set_style_bg_opa(s_pwd_btn_connect, LV_OPA_40, 0);
    lv_obj_align(s_pwd_btn_connect, LV_ALIGN_TOP_LEFT, 2, 76);
    lv_obj_add_event_cb(s_pwd_btn_connect, pwd_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    lbl = lv_label_create(s_pwd_btn_connect);
    lv_label_set_text(lbl, "Connect");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    s_btn_eye = lv_btn_create(s_pwd_view);
    lv_obj_set_size(s_btn_eye, 30, 28);
    lv_obj_set_style_bg_color(s_btn_eye, COLOR_PANEL, 0);
    lv_obj_set_style_radius(s_btn_eye, 5, 0);
    lv_obj_align(s_btn_eye, LV_ALIGN_TOP_LEFT, 84, 76);
    lv_obj_add_event_cb(s_btn_eye, pwd_eye_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(s_btn_eye);
    lv_label_set_text(lbl, LV_SYMBOL_EYE_CLOSE);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    /* 提示文本（密码过短等） */
    s_pwd_hint = lv_label_create(s_pwd_view);
    lv_label_set_text(s_pwd_hint, "");
    lv_obj_set_style_text_color(s_pwd_hint, COLOR_TEXT_DIM, 0);
    lv_label_set_long_mode(s_pwd_hint, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_pwd_hint, SCREEN_W - 8);
    lv_obj_align(s_pwd_hint, LV_ALIGN_TOP_MID, 0, 110);

    /* 简化软键盘（视图底部固定，随视图整体显示/隐藏）：
     * 数字行常驻 + QWERTY 字母，Shift 切换大小写，无符号页；
     * 4 行 × 42px 大按键，16px 字体 */
    s_pwd_kb = lv_btnmatrix_create(s_pwd_view);
    s_kb_upper = false;
    kb_apply_shift(s_pwd_kb);
    lv_obj_set_size(s_pwd_kb, SCREEN_W, 168);
    lv_obj_align(s_pwd_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(s_pwd_kb, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_opa(s_pwd_kb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pwd_kb, 0, 0);
    lv_obj_set_style_pad_all(s_pwd_kb, 1, 0);
    lv_obj_set_style_pad_gap(s_pwd_kb, 2, 0);
    /* 按键样式与页面配色一致 */
    lv_obj_set_style_bg_color(s_pwd_kb, COLOR_PANEL, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_pwd_kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_pwd_kb, COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_pwd_kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(s_pwd_kb, 5, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(s_pwd_kb, 0, LV_PART_ITEMS);
    /* Shift 激活（大写）高亮 */
    lv_obj_set_style_bg_color(s_pwd_kb, COLOR_HOT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(s_pwd_kb, COLOR_BG, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_pwd_kb, pwd_kb_cb, LV_EVENT_ALL, NULL);
}

static void pwd_dialog_open(const char *ssid) {
    if (!s_pwd_view) return;
    strncpy(s_pwd_ssid, ssid, sizeof(s_pwd_ssid) - 1);
    s_pwd_ssid[sizeof(s_pwd_ssid) - 1] = '\0';
    lv_label_set_text(s_pwd_ssid_label, ssid);
    lv_textarea_set_text(s_pwd_ta, "");
    lv_textarea_set_password_mode(s_pwd_ta, true);
    s_pwd_show = false;
    lv_label_set_text(lv_obj_get_child(s_btn_eye, 0), LV_SYMBOL_EYE_CLOSE);
    lv_label_set_text(s_pwd_hint, "");
    lv_obj_set_style_bg_opa(s_pwd_btn_connect, LV_OPA_40, 0);
    /* 隐藏列表：全页输入，无叠加透出 */
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pwd_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_pwd_view);
    /* 键盘重置为小写 */
    s_kb_upper = false;
    kb_apply_shift(s_pwd_kb);
}

/* ============ 列表项 ============ */

static void item_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || (size_t)idx >= s_item_count) return;

    const wifi_item_t *it = &s_items[idx];

    if (wifi_manager_get_state() == WIFI_MANAGER_DISABLED) {
        lv_label_set_text(s_status_label, "WiFi OFF - enable in Settings");
        lv_obj_set_style_text_color(s_status_label, COLOR_RED, 0);
        return;
    }
    if (it->connected || it->connecting) return; /* 已连接/连接中不可点 */

    if (it->saved) {
        /* 已保存：直接用保存的密码连接 */
        wifi_manager_connect_saved(it->ssid);
        wifi_ui_refresh_now();
    } else if (is_open_net(it->auth_mode)) {
        /* 开放网络：无密码直连（成功后由 manager 保存） */
        wifi_manager_connect(it->ssid, "");
        wifi_ui_refresh_now();
    } else {
        /* 加密网络：弹密码输入 */
        pwd_dialog_open(it->ssid);
    }
}

static void rebuild_list(void) {
    if (!s_list) return;
    lv_obj_clean(s_list);

    s_item_count = wifi_manager_get_items(s_items, WIFI_MANAGER_MAX_ITEMS);

    for (size_t i = 0; i < s_item_count; i++) {
        const wifi_item_t *it = &s_items[i];

        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_size(item, SCREEN_W - 8, ITEM_H);
        obj_set_pad_all(item, 4);
        lv_obj_set_style_radius(item, 6, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_align(item, LV_ALIGN_TOP_MID, 0, (lv_coord_t)(i * (ITEM_H + ITEM_GAP)));

        if (it->connected) {
            lv_obj_set_style_bg_color(item, COLOR_CONN_BG, 0);
            lv_obj_set_style_border_width(item, 2, 0);
            lv_obj_set_style_border_color(item, COLOR_GREEN, 0);
        } else if (it->connecting) {
            lv_obj_set_style_bg_color(item, COLOR_PANEL, 0);
            lv_obj_set_style_border_width(item, 2, 0);
            lv_obj_set_style_border_color(item, COLOR_HOT, 0);
        } else if (it->saved) {
            lv_obj_set_style_bg_color(item, COLOR_PANEL_LT, 0);
            lv_obj_set_style_border_width(item, 0, 0);
        } else {
            lv_obj_set_style_bg_color(item, COLOR_PANEL, 0);
            lv_obj_set_style_border_width(item, 0, 0);
        }

        /* 加密标记 */
        if (!is_open_net(it->auth_mode)) {
            create_lock_icon(item);
        }

        /* SSID（长名截断为省略号） */
        lv_obj_t *ssid = lv_label_create(item);
        lv_label_set_text(ssid, it->ssid);
        lv_obj_set_style_text_color(ssid, COLOR_TEXT, 0);
        lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
        lv_obj_set_width(ssid, 92);
        lv_obj_align(ssid, LV_ALIGN_TOP_LEFT, 22, 0);

        /* 信号条 */
        create_signal_bars(item, rssi_to_bars(it->rssi));

        /* 第二行状态（宽度收窄，给右侧信号条留安全边距） */
        lv_obj_t *line2 = lv_label_create(item);
        lv_obj_set_width(line2, 100);
        lv_label_set_long_mode(line2, LV_LABEL_LONG_DOT);
        lv_obj_align(line2, LV_ALIGN_BOTTOM_LEFT, 22, 0);
        if (it->connected) {
            char ip[16] = "";
            wifi_manager_get_ip(ip, sizeof(ip));
            lv_label_set_text_fmt(line2, LV_SYMBOL_OK " Connected  %s", ip);
            lv_obj_set_style_text_color(line2, COLOR_GREEN, 0);
        } else if (it->connecting) {
            lv_label_set_text(line2, "Connecting...");
            lv_obj_set_style_text_color(line2, COLOR_HOT, 0);
        } else if (it->saved) {
            lv_label_set_text_fmt(line2, "Saved  %s", auth_to_str(it->auth_mode));
            lv_obj_set_style_text_color(line2, COLOR_TEXT_DIM, 0);
        } else {
            lv_label_set_text_fmt(line2, "%s", auth_to_str(it->auth_mode));
            lv_obj_set_style_text_color(line2, COLOR_TEXT_DIM, 0);
        }
    }

    /* 内容总高度（供滚动范围） */
    lv_obj_set_height(s_list, SCREEN_H - LIST_Y);
}

/* ============ 状态行 / 标题栏刷新 ============ */

static void refresh_header(void) {
    wifi_manager_state_t st = wifi_manager_get_state();
    bool scanning = wifi_manager_is_scanning();

    /* 状态行 */
    if (st == WIFI_MANAGER_DISABLED) {
        lv_label_set_text(s_status_label, "WiFi OFF - enable in Settings");
        lv_obj_set_style_text_color(s_status_label, COLOR_RED, 0);
    } else if (scanning) {
        lv_label_set_text(s_status_label, "Scanning...");
        lv_obj_set_style_text_color(s_status_label, COLOR_HOT, 0);
    } else if (st == WIFI_MANAGER_CONNECTING) {
        /* 连接目标在列表项上有 "Connecting..." 标记 */
        lv_label_set_text(s_status_label, "Connecting...");
        lv_obj_set_style_text_color(s_status_label, COLOR_HOT, 0);
    } else if (st == WIFI_MANAGER_CONNECTED) {
        char ssid[33] = "";
        wifi_manager_get_connected_ssid(ssid, sizeof(ssid));
        lv_label_set_text_fmt(s_status_label, LV_SYMBOL_OK " %s", ssid[0] ? ssid : "Connected");
        lv_obj_set_style_text_color(s_status_label, COLOR_GREEN, 0);
    } else if (st == WIFI_MANAGER_FAILED) {
        char reason[32] = "";
        wifi_manager_get_fail_reason(reason, sizeof(reason));
        lv_label_set_text_fmt(s_status_label, "Failed: %s", reason);
        lv_obj_set_style_text_color(s_status_label, COLOR_RED, 0);
    } else {
        lv_label_set_text_fmt(s_status_label, "%d network%s%s",
                              (int)s_item_count,
                              s_item_count == 1 ? "" : "s",
                              s_item_count == 0 ? " - tap scan" : "");
        lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    }

    /* 重扫描按钮：扫描中/连接中/关闭时置灰 */
    bool busy = scanning || st == WIFI_MANAGER_CONNECTING || st == WIFI_MANAGER_DISABLED;
    lv_obj_set_style_bg_color(s_btn_rescan, busy ? COLOR_PANEL : COLOR_PANEL_LT, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(s_btn_rescan, 0),
                                busy ? COLOR_TEXT_DIM : COLOR_HOT, 0);
}

/* ============ 对外接口 ============ */

void wifi_ui_refresh_now(void) {
    if (!s_visible || !s_page) return;
    s_last_version = wifi_manager_get_version();
    rebuild_list();
    refresh_header();
}

static void poll_timer_cb(lv_timer_t *timer) {
    (void)timer;
    if (!s_visible) return;
    uint32_t v = wifi_manager_get_version();
    if (v != s_last_version) {
        wifi_ui_refresh_now();
    }
}

static void btn_back_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    wifi_ui_show(false);
}

static void btn_rescan_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;
    esp_err_t err = wifi_manager_scan_start();
    if (err == ESP_OK) {
        wifi_ui_refresh_now();
    } else if (err == ESP_ERR_INVALID_STATE) {
        lv_label_set_text(s_status_label, "Busy or WiFi off");
        lv_obj_set_style_text_color(s_status_label, COLOR_RED, 0);
    }
}

void wifi_ui_init(void) {
    if (s_page) return;

    s_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_page, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_page, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_page, 0, 0);
    lv_obj_set_style_radius(s_page, 0, 0);
    obj_set_pad_all(s_page, 0);
    lv_obj_align(s_page, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_page, LV_OBJ_FLAG_HIDDEN);

    /* 标题栏：返回 / 标题 / 重扫描 */
    s_btn_back = lv_obj_create(s_page);
    lv_obj_set_size(s_btn_back, 28, 22);
    style_btn(s_btn_back);
    lv_obj_align(s_btn_back, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_add_flag(s_btn_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(s_btn_back);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_center(lbl);

    lv_obj_t *title = lv_label_create(s_page);
    lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi");
    lv_obj_set_style_text_color(title, COLOR_HOT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 7);

    s_btn_rescan = lv_obj_create(s_page);
    lv_obj_set_size(s_btn_rescan, 28, 22);
    style_btn(s_btn_rescan);
    lv_obj_align(s_btn_rescan, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_add_flag(s_btn_rescan, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_btn_rescan, btn_rescan_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(s_btn_rescan);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(lbl, COLOR_HOT, 0);
    lv_obj_center(lbl);

    /* 状态行 */
    s_status_label = lv_label_create(s_page);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, SCREEN_W - 8);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, HEADER_H + 1);

    /* 可滚动列表 */
    s_list = lv_obj_create(s_page);
    lv_obj_set_size(s_list, SCREEN_W, SCREEN_H - LIST_Y);
    lv_obj_set_style_bg_color(s_list, COLOR_BG, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    obj_set_pad_all(s_list, 2);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, LIST_Y);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

    /* 密码输入弹窗（默认隐藏） */
    create_pwd_dialog();

    ESP_LOGI(TAG, "WiFi UI initialized");
}

void wifi_ui_show(bool show) {
    if (!s_page) return;

    if (show) {
        s_visible = true;
        lv_obj_clear_flag(s_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_page);
        pwd_dialog_close();

        /* 无扫描结果且空闲时自动发起扫描 */
        s_item_count = wifi_manager_get_items(s_items, WIFI_MANAGER_MAX_ITEMS);
        wifi_manager_state_t st = wifi_manager_get_state();
        if (s_item_count == 0 && st == WIFI_MANAGER_IDLE && !wifi_manager_is_scanning()) {
            wifi_manager_scan_start();
        }
        wifi_ui_refresh_now();

        if (!s_poll_timer) {
            s_poll_timer = lv_timer_create(poll_timer_cb, 250, NULL);
        }
        lv_timer_resume(s_poll_timer);
    } else {
        s_visible = false;
        pwd_dialog_close();
        lv_obj_add_flag(s_page, LV_OBJ_FLAG_HIDDEN);
        if (s_poll_timer) {
            lv_timer_pause(s_poll_timer);
        }
    }
}

bool wifi_ui_is_visible(void) {
    return s_visible;
}
