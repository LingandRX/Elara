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
#include "buddy_theme.h"

static const char *TAG = "WIFI_UI";

/* ============ 颜色（复用 buddy Warm 主题；不重复定义主题色） ============ */
#define COLOR_PANEL_LT COLOR_PANEL2
#define COLOR_HOT      COLOR_ACCENT
#define COLOR_WARN     COLOR_WARM
#define COLOR_CONN_BG  COLOR_PANEL2
#define COLOR_BAR_OFF  COLOR_LINE

/* ============ 布局常量 ============ */
#define SCREEN_W   170
#define SCREEN_H   320
#define HEADER_H   30          /* 标题栏 */
#define CONN_CARD_Y 34         /* 已连接卡片 Y */
#define CONN_CARD_H 52         /* 已连接卡片高 */
#define GROUP_Y    (CONN_CARD_Y + CONN_CARD_H + 8)   /* 「可用网络」分组标题 Y */
#define GROUP_H    14
#define LIST_Y     (GROUP_Y + GROUP_H)               /* 列表起始 Y */
#define ITEM_H     40
#define ITEM_GAP   4

/* ============ 页面对象 ============ */
static lv_obj_t *s_page = NULL;        /* 全屏页面（挂在 lv_scr_act） */
static lv_obj_t *s_btn_back = NULL;
static lv_obj_t *s_status_label = NULL;   /* 右上状态文字（可点击重扫描） */
static lv_obj_t *s_conn_card = NULL;     /* 已连接卡片 */
static lv_obj_t *s_conn_ssid = NULL;
static lv_obj_t *s_conn_meta = NULL;
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




static bool is_open_net(wifi_auth_mode_t auth) {
    return auth == WIFI_AUTH_OPEN;
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
        lv_label_set_text(s_pwd_hint, "密码至少 8 位");
        lv_obj_set_style_text_color(s_pwd_hint, COLOR_WARN, 0);
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
    lv_obj_set_style_text_font(s_pwd_ssid_label, FONT_MD, 0);
    lv_label_set_long_mode(s_pwd_ssid_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_pwd_ssid_label, SCREEN_W - 38);
    lv_obj_align(s_pwd_ssid_label, LV_ALIGN_TOP_LEFT, 36, 9);

    /* 密码输入框 */
    s_pwd_ta = lv_textarea_create(s_pwd_view);
    lv_obj_set_size(s_pwd_ta, SCREEN_W - 4, 34);
    lv_textarea_set_one_line(s_pwd_ta, true);
    lv_textarea_set_password_mode(s_pwd_ta, true);
    lv_textarea_set_max_length(s_pwd_ta, 64);   /* ESP32 WiFi 密码上限，防溢出 */
    lv_textarea_set_placeholder_text(s_pwd_ta, "密码 (8-64)");
    lv_obj_set_style_bg_color(s_pwd_ta, COLOR_PANEL, 0);
    lv_obj_set_style_text_color(s_pwd_ta, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_pwd_ta, FONT_MD, 0);
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
    lv_label_set_text(lbl, "连接");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_MD, 0);
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
    lv_obj_set_style_text_font(s_pwd_hint, FONT_SM, 0);
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
        lv_label_set_text(s_status_label, "WiFi 已关闭，请在设置中开启");
        lv_obj_set_style_text_color(s_status_label, COLOR_WARN, 0);
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

static const char *rssi_to_label(int8_t rssi) {
    if (rssi >= -60) return "强";
    if (rssi >= -70) return "中";
    return "弱";
}

static void rebuild_list(void) {
    if (!s_list) return;
    lv_obj_clean(s_list);

    s_item_count = wifi_manager_get_items(s_items, WIFI_MANAGER_MAX_ITEMS);

    for (size_t i = 0; i < s_item_count; i++) {
        const wifi_item_t *it = &s_items[i];

        lv_obj_t *item = lv_obj_create(s_list);
        lv_obj_set_size(item, SCREEN_W - 2 * PAGE_PAD_X, ITEM_H);
        obj_set_pad_all(item, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_align(item, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, (lv_coord_t)(i * (ITEM_H + ITEM_GAP)));

        /* 当前已连接/已保存项用 PANEL2 底高亮（见设计文档 §5.6） */
        if (it->connected || it->saved) {
            lv_obj_set_style_bg_color(item, COLOR_PANEL2, 0);
        } else {
            lv_obj_set_style_bg_color(item, COLOR_PANEL, 0);
        }

        /* SSID */
        lv_obj_t *ssid = lv_label_create(item);
        lv_label_set_text(ssid, it->ssid);
        lv_obj_set_style_text_color(ssid, COLOR_TEXT, 0);
        lv_obj_set_style_text_font(ssid, FONT_MD, 0);
        lv_label_set_long_mode(ssid, LV_LABEL_LONG_DOT);
        lv_obj_set_width(ssid, 100);
        lv_obj_align(ssid, LV_ALIGN_LEFT_MID, 12, 0);

        /* 右侧信号档位：强 / 中 / 弱 */
        lv_obj_t *sig = lv_label_create(item);
        if (it->connecting) {
            lv_label_set_text(sig, "连接中");
            lv_obj_set_style_text_color(sig, COLOR_ACCENT, 0);
        } else {
            lv_label_set_text(sig, rssi_to_label(it->rssi));
            lv_obj_set_style_text_color(sig, COLOR_TEXT_DIM, 0);
        }
        lv_obj_set_style_text_font(sig, FONT_SM, 0);
        lv_obj_align(sig, LV_ALIGN_RIGHT_MID, -10, 0);
    }

    /* 内容总高度（供滚动范围） */
    lv_obj_set_height(s_list, SCREEN_H - LIST_Y);
}

/* ============ 状态行 / 标题栏刷新 ============ */

static void refresh_header(void) {
    wifi_manager_state_t st = wifi_manager_get_state();
    bool scanning = wifi_manager_is_scanning();

    /* 右上状态文字 */
    if (st == WIFI_MANAGER_DISABLED) {
        lv_label_set_text(s_status_label, "WiFi 已关闭");
        lv_obj_set_style_text_color(s_status_label, COLOR_WARN, 0);
    } else if (scanning) {
        lv_label_set_text(s_status_label, "扫描中…");
        lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    } else if (st == WIFI_MANAGER_CONNECTING) {
        lv_label_set_text(s_status_label, "连接中…");
        lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    } else if (st == WIFI_MANAGER_CONNECTED) {
        lv_label_set_text(s_status_label, "已连接");
        lv_obj_set_style_text_color(s_status_label, COLOR_ACCENT, 0);
    } else if (st == WIFI_MANAGER_FAILED) {
        lv_label_set_text(s_status_label, "连接失败");
        lv_obj_set_style_text_color(s_status_label, COLOR_WARN, 0);
    } else {
        lv_label_set_text(s_status_label, "点击扫描");
        lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    }

    /* 已连接卡片：SSID + 「信号 -52dBm · 已保存」 */
    if (st == WIFI_MANAGER_CONNECTED) {
        char ssid[33] = "";
        wifi_manager_get_connected_ssid(ssid, sizeof(ssid));
        int8_t rssi = 0;
        bool has_rssi = wifi_manager_get_rssi(&rssi);

        lv_obj_clear_flag(s_conn_card, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_conn_ssid, ssid[0] ? ssid : "已连接");
        if (has_rssi) {
            lv_label_set_text_fmt(s_conn_meta, "信号 %ddBm · 已保存", (int)rssi);
        } else {
            lv_label_set_text(s_conn_meta, "已保存");
        }
    } else {
        lv_obj_add_flag(s_conn_card, LV_OBJ_FLAG_HIDDEN);
    }
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
        lv_label_set_text(s_status_label, "忙或 WiFi 已关闭");
        lv_obj_set_style_text_color(s_status_label, COLOR_WARN, 0);
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

    /* 标题栏：返回箭头 / 标题「网络」 / 右上状态（可点重扫描） */
    s_btn_back = lv_obj_create(s_page);
    lv_obj_set_size(s_btn_back, 24, 20);
    lv_obj_set_style_bg_opa(s_btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_btn_back, 0, 0);
    obj_set_pad_all(s_btn_back, 0);
    lv_obj_align(s_btn_back, LV_ALIGN_TOP_LEFT, 4, 5);
    lv_obj_add_flag(s_btn_back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(s_btn_back);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_center(lbl);

    lv_obj_t *title = lv_label_create(s_page);
    lv_label_set_text(title, "网络");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_MD, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 30, 7);

    /* 右上状态文字（点击 = 重新扫描） */
    s_status_label = lv_label_create(s_page);
    lv_label_set_text(s_status_label, "");
    lv_obj_set_style_text_color(s_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_status_label, FONT_SM, 0);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status_label, 96);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_RIGHT, -PAGE_PAD_X, 8);
    lv_obj_add_flag(s_status_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_status_label, btn_rescan_cb, LV_EVENT_CLICKED, NULL);

    /* 标题栏分隔线 */
    lv_obj_t *hsep = lv_obj_create(s_page);
    lv_obj_set_size(hsep, SCREEN_W - 2 * PAGE_PAD_X, 1);
    lv_obj_set_style_bg_color(hsep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(hsep, 0, 0);
    lv_obj_set_style_radius(hsep, 0, 0);
    lv_obj_clear_flag(hsep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(hsep, LV_ALIGN_TOP_MID, 0, HEADER_H - 1);

    /* 已连接卡片：SSID + 「信号 -52dBm · 已保存」 */
    s_conn_card = lv_obj_create(s_page);
    lv_obj_set_size(s_conn_card, SCREEN_W - 2 * PAGE_PAD_X, CONN_CARD_H);
    lv_obj_set_style_bg_color(s_conn_card, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(s_conn_card, 0, 0);
    lv_obj_set_style_radius(s_conn_card, 10, 0);
    obj_set_pad_all(s_conn_card, 0);
    lv_obj_align(s_conn_card, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, CONN_CARD_Y);
    lv_obj_clear_flag(s_conn_card, LV_OBJ_FLAG_SCROLLABLE);

    s_conn_ssid = lv_label_create(s_conn_card);
    lv_label_set_text(s_conn_ssid, "--");
    lv_obj_set_style_text_color(s_conn_ssid, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_conn_ssid, FONT_MD, 0);
    lv_obj_align(s_conn_ssid, LV_ALIGN_TOP_LEFT, 10, 9);

    s_conn_meta = lv_label_create(s_conn_card);
    lv_label_set_text(s_conn_meta, "");
    lv_obj_set_style_text_color(s_conn_meta, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_conn_meta, FONT_SM, 0);
    lv_obj_align(s_conn_meta, LV_ALIGN_TOP_LEFT, 10, 29);

    /* 「可用网络」分组标题 */
    lv_obj_t *grp = lv_label_create(s_page);
    lv_label_set_text(grp, "可用网络");
    lv_obj_set_style_text_color(grp, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(grp, FONT_SM, 0);
    lv_obj_align(grp, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, GROUP_Y);

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
