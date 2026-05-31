/**
 * @file lvgl_chat_ui.c
 * LVGL 聊天界面实现
 */

#include "lvgl_chat_ui.h"
#include "pet_anim.h"
#include "pet_ui.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "LVGL_CHAT_UI";

/* 最大消息数量 */
#define MAX_MESSAGES 20

/* UI 颜色定义 */
#define COLOR_BG           lv_color_hex(0x080810)      /* 深蓝灰背景 */
#define COLOR_USER_BUB     lv_color_hex(0x007ACC)      /* 用户蓝色 */
#define COLOR_AI_BUB       lv_color_hex(0x2D2D37)      /* AI 深灰 */
#define COLOR_SYS_BUB      lv_color_hex(0x464650)      /* 系统灰 */
#define COLOR_TEXT         lv_color_hex(0xFFFFFF)       /* 白色文字 */
#define COLOR_TEXT_DIM     lv_color_hex(0xA0A0AA)       /* 暗淡文字 */
#define COLOR_LABEL_AI     lv_color_hex(0x64B4FF)       /* AI 标签色 */
#define COLOR_LABEL_USER   lv_color_hex(0x78DCB4)       /* 用户标签色 */
#define COLOR_STATUS_IDLE  lv_color_hex(0x00C864)       /* 空闲绿 */
#define COLOR_STATUS_LISTEN lv_color_hex(0xFFC800)      /* 聆听黄 */
#define COLOR_STATUS_THINK lv_color_hex(0xFF8C00)       /* 思考橙 */
#define COLOR_STATUS_REPLY lv_color_hex(0xB464FF)       /* 回复紫 */
#define COLOR_STATUS_ERR   lv_color_hex(0xFF3C3C)       /* 错误红 */

/* UI 元素 */
static lv_obj_t *main_container = NULL;
static lv_obj_t *status_bar = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *emotion_indicator = NULL;
static lv_obj_t *chat_area = NULL;
static lv_obj_t *bottom_bar = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *wifi_status_label = NULL;

/* Wi-Fi 页面元素与状态 */
static lv_obj_t *wifi_page_obj = NULL;
static lv_obj_t *wifi_info_label = NULL;
static bool s_wifi_connected = false;
static char s_saved_ssid[64] = "";

/* 消息列表 */
static ChatMessage messages[MAX_MESSAGES];
static int msg_count = 0;
static int msg_head = 0;

/* 当前情绪 */
static char current_emotion[16] = "";

/* 辅助函数：v8 中没有 obj_set_pad_all */
static inline void obj_set_pad_all(lv_obj_t *obj, lv_coord_t pad, lv_style_selector_t selector) {
    lv_obj_set_style_pad_left(obj, pad, selector);
    lv_obj_set_style_pad_right(obj, pad, selector);
    lv_obj_set_style_pad_top(obj, pad, selector);
    lv_obj_set_style_pad_bottom(obj, pad, selector);
}

/* 动画辅助：v8 中 lv_anim_exec_xcb_t 只接受 2 个参数 */
static void anim_opa_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa(obj, v, 0);
}

/* 前向声明 */
static lv_obj_t *create_message_bubble(MsgRole role, const char *text);
static lv_obj_t *create_animated_message_bubble(MsgRole role, const char *text);

/**
 * 创建状态栏
 */
static void create_status_bar(void) {
    status_bar = lv_obj_create(main_container);
    lv_obj_set_size(status_bar, LV_HOR_RES, 32);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    obj_set_pad_all(status_bar, 6, 0);
    lv_obj_align(status_bar, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 情绪指示器 */
    emotion_indicator = lv_obj_create(status_bar);
    lv_obj_set_size(emotion_indicator, 12, 12);
    lv_obj_set_style_bg_color(emotion_indicator, COLOR_STATUS_IDLE, 0);
    lv_obj_set_style_radius(emotion_indicator, 6, 0);
    lv_obj_set_style_border_width(emotion_indicator, 0, 0);
    lv_obj_align(emotion_indicator, LV_ALIGN_LEFT_MID, 4, 0);

    /* 状态文本 */
    status_label = lv_label_create(status_bar);
    lv_label_set_text(status_label, "Idle");
    lv_obj_set_style_text_color(status_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(status_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 22, 0);

    /* Wi-Fi 状态文本/图标 */
    wifi_status_label = lv_label_create(status_bar);
    lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(wifi_status_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_RIGHT_MID, -6, 0);
}

/**
 * 创建聊天区域
 */
static void create_chat_area(void) {
    chat_area = lv_obj_create(main_container);
    lv_obj_set_size(chat_area, LV_HOR_RES, LV_VER_RES - 56); /* 减去状态栏(32)和底部栏(24) */
    lv_obj_set_style_bg_color(chat_area, COLOR_BG, 0);
    lv_obj_set_style_border_width(chat_area, 0, 0);
    lv_obj_set_style_radius(chat_area, 0, 0);
    obj_set_pad_all(chat_area, 4, 0);
    lv_obj_align(chat_area, LV_ALIGN_TOP_LEFT, 0, 32);  /* 状态栏高度 32 */
    lv_obj_set_scroll_dir(chat_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(chat_area, LV_SCROLLBAR_MODE_AUTO);
}

/**
 * 创建进度条
 */
static void create_progress_bar(void) {
    progress_bar = lv_bar_create(main_container);
    lv_obj_set_size(progress_bar, LV_HOR_RES - 40, 6);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x2D2D37), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar, COLOR_LABEL_AI, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(progress_bar, 3, LV_PART_INDICATOR);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */
}

/**
 * 创建底部栏
 */
static void create_bottom_bar(void) {
    bottom_bar = lv_obj_create(main_container);
    lv_obj_set_size(bottom_bar, LV_HOR_RES, 24);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    obj_set_pad_all(bottom_bar, 4, 0);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* 底部文本 */
    lv_obj_t *bottom_label = lv_label_create(bottom_bar);
    lv_label_set_text(bottom_label, "Elara Chat");
    lv_obj_set_style_text_color(bottom_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(bottom_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(bottom_label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * 创建 Wi-Fi 设置页面
 */
static void create_wifi_page(void) {
    wifi_page_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifi_page_obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(wifi_page_obj, COLOR_BG, 0);
    lv_obj_add_flag(wifi_page_obj, LV_OBJ_FLAG_HIDDEN); /* 默认隐藏 */
    lv_obj_set_style_border_width(wifi_page_obj, 0, 0);
    lv_obj_set_style_radius(wifi_page_obj, 0, 0);
    obj_set_pad_all(wifi_page_obj, 0, 0);

    lv_obj_t *title = lv_label_create(wifi_page_obj);
    lv_label_set_text(title, LV_SYMBOL_WIFI " Wi-Fi Settings");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *inst = lv_label_create(wifi_page_obj);
    lv_label_set_text(inst, "To configure Wi-Fi, open serial\nterminal and send command:\n\nwifi <ssid> <password>\n\nPress BOOT button to exit.");
    lv_obj_set_style_text_color(inst, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(inst, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(inst, LV_ALIGN_CENTER, 0, -10);

    wifi_info_label = lv_label_create(wifi_page_obj);
    lv_label_set_text(wifi_info_label, "SSID: None\nStatus: Disconnected");
    lv_obj_set_style_text_color(wifi_info_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_align(wifi_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(wifi_info_label, LV_ALIGN_BOTTOM_MID, 0, -40);
}

/**
 * 创建消息气泡
 */
static lv_obj_t *create_message_bubble(MsgRole role, const char *text) {
    lv_obj_t *bubble = lv_obj_create(chat_area);
    lv_obj_set_style_radius(bubble, 8, 0);
    obj_set_pad_all(bubble, 8, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_width(bubble, LV_HOR_RES - 24);

    /* 根据角色设置颜色 */
    switch (role) {
        case MSG_ROLE_USER:
            lv_obj_set_style_bg_color(bubble, COLOR_USER_BUB, 0);
            lv_obj_align(bubble, LV_ALIGN_TOP_RIGHT, -4, 0);
            break;
        case MSG_ROLE_AI:
            lv_obj_set_style_bg_color(bubble, COLOR_AI_BUB, 0);
            lv_obj_align(bubble, LV_ALIGN_TOP_LEFT, 4, 0);
            break;
        case MSG_ROLE_SYSTEM:
            lv_obj_set_style_bg_color(bubble, COLOR_SYS_BUB, 0);
            lv_obj_align(bubble, LV_ALIGN_TOP_MID, 0, 0);
            break;
    }

    /* 角色标签 */
    lv_obj_t *role_label = lv_label_create(bubble);
    switch (role) {
        case MSG_ROLE_USER:
            lv_label_set_text(role_label, "You");
            lv_obj_set_style_text_color(role_label, COLOR_LABEL_USER, 0);
            break;
        case MSG_ROLE_AI:
            lv_label_set_text(role_label, "AI");
            lv_obj_set_style_text_color(role_label, COLOR_LABEL_AI, 0);
            break;
        case MSG_ROLE_SYSTEM:
            lv_label_set_text(role_label, "System");
            lv_obj_set_style_text_color(role_label, COLOR_TEXT_DIM, 0);
            break;
    }
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

    /* 调整气泡高度以适应内容 */
    lv_obj_update_layout(bubble);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    return bubble;
}

/**
 * 初始化聊天界面
 */
void lvgl_chat_ui_init(void) {
    ESP_LOGI(TAG, "Initializing LVGL chat UI...");

    /* 清空消息 */
    msg_count = 0;
    msg_head = 0;
    memset(messages, 0, sizeof(messages));

    /* 创建主容器 */
    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(main_container, COLOR_BG, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_radius(main_container, 0, 0);
    obj_set_pad_all(main_container, 0, 0);
    lv_obj_align(main_container, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 创建 UI 元素 */
    create_status_bar();
    create_chat_area();
    create_progress_bar();
    create_bottom_bar();
    create_wifi_page();

    ESP_LOGI(TAG, "LVGL chat UI initialized");
}

/**
 * 添加消息
 */
void lvgl_chat_ui_add_msg(const char *role, const char *text, const char *emotion, bool is_chunk) {
    if (!role || !text) return;

    /* 解析角色 */
    MsgRole msg_role;
    if (strcmp(role, "user") == 0) {
        msg_role = MSG_ROLE_USER;
    } else if (strcmp(role, "ai") == 0) {
        msg_role = MSG_ROLE_AI;
    } else {
        msg_role = MSG_ROLE_SYSTEM;
    }

    /* 如果是流式分片且最后一条消息是同一角色，则追加 */
    if (is_chunk && msg_count > 0) {
        ChatMessage *last_msg = &messages[(msg_head + msg_count - 1) % MAX_MESSAGES];
        if (last_msg->role == msg_role && last_msg->is_chunk) {
            /* 追加文本 */
            size_t remaining = sizeof(last_msg->text) - strlen(last_msg->text) - 1;
            if (remaining > 0) {
                strncat(last_msg->text, text, remaining);
            }
            /* 更新情绪 */
            if (emotion && strlen(emotion) > 0) {
                strncpy(last_msg->emotion, emotion, sizeof(last_msg->emotion) - 1);
            }
            /* 重新渲染最后一条消息 */
            if (chat_area) {
                /* 删除最后一个子对象 (消息气泡) */
                uint32_t child_count = lv_obj_get_child_cnt(chat_area);
                if (child_count > 0) {
                    lv_obj_del(lv_obj_get_child(chat_area, child_count - 1));
                }
                /* 重新创建消息气泡 */
                create_message_bubble(msg_role, last_msg->text);
                /* 滚动到底部 */
                lv_obj_scroll_to_y(chat_area, LV_COORD_MAX, LV_ANIM_ON);
            }
            return;
        }
    }

    /* 添加新消息 */
    ChatMessage *msg = &messages[(msg_head + msg_count) % MAX_MESSAGES];
    msg->role = msg_role;
    strncpy(msg->text, text, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';
    if (emotion) {
        strncpy(msg->emotion, emotion, sizeof(msg->emotion) - 1);
        msg->emotion[sizeof(msg->emotion) - 1] = '\0';
    } else {
        msg->emotion[0] = '\0';
    }
    msg->is_chunk = is_chunk;

    /* 更新消息计数 */
    if (msg_count < MAX_MESSAGES) {
        msg_count++;
    } else {
        /* 消息已满，删除最旧的 */
        msg_head = (msg_head + 1) % MAX_MESSAGES;
    }

    /* 更新情绪显示 */
    if (emotion && strlen(emotion) > 0) {
        strncpy(current_emotion, emotion, sizeof(current_emotion) - 1);
        current_emotion[sizeof(current_emotion) - 1] = '\0';
        /* 更新情绪指示器颜色 */
        if (emotion_indicator) {
            if (strcmp(emotion, "happy") == 0) {
                lv_obj_set_style_bg_color(emotion_indicator, COLOR_STATUS_IDLE, 0);
            } else if (strcmp(emotion, "sad") == 0) {
                lv_obj_set_style_bg_color(emotion_indicator, COLOR_STATUS_LISTEN, 0);
            } else {
                lv_obj_set_style_bg_color(emotion_indicator, COLOR_STATUS_THINK, 0);
            }
        }
    }

    /* 创建带动画的消息气泡 */
    create_animated_message_bubble(msg_role, text);

    /* 滚动到底部 */
    lv_obj_scroll_to_y(chat_area, LV_COORD_MAX, LV_ANIM_ON);
}

/**
 * 设置状态
 */
void lvgl_chat_ui_set_status(const char *state, const char *emotion) {
    if (!state) return;

    /* 更新状态文本 */
    if (status_label) {
        lv_label_set_text(status_label, state);
    }

    /* 映射系统状态到 Pet 动画 */
    if (strcasecmp(state, "idle") == 0) {
        pet_ui_set_state(PET_ANIM_IDLE);
    } else if (strcasecmp(state, "listening") == 0) {
        pet_ui_set_state(PET_ANIM_WAVING); /* 聆听时挥手 */
    } else if (strcasecmp(state, "thinking") == 0) {
        pet_ui_set_state(PET_ANIM_REVIEW); /* 思考时执行 Inspect/Think */
    } else if (strcasecmp(state, "replying") == 0) {
        pet_ui_set_state(PET_ANIM_RUNNING); /* 回复时执行 Action/Work */
    } else if (strcasecmp(state, "error") == 0 || strcasecmp(state, "failed") == 0) {
        pet_ui_set_state(PET_ANIM_FAILED); /* 错误时执行 Fail/Hurt */
    }

    /* 更新情绪指示器 */
    if (emotion_indicator) {
        lv_color_t color = COLOR_STATUS_IDLE;
        if (strcasecmp(state, "idle") == 0) {
            color = COLOR_STATUS_IDLE;
        } else if (strcasecmp(state, "listening") == 0) {
            color = COLOR_STATUS_LISTEN;
        } else if (strcasecmp(state, "thinking") == 0) {
            color = COLOR_STATUS_THINK;
        } else if (strcasecmp(state, "replying") == 0) {
            color = COLOR_STATUS_REPLY;
        } else if (strcasecmp(state, "error") == 0 || strcasecmp(state, "failed") == 0) {
            color = COLOR_STATUS_ERR;
        } else if (strcasecmp(state, "running") == 0 || strcasecmp(state, "run right") == 0 || strcasecmp(state, "run left") == 0) {
            color = COLOR_STATUS_REPLY;
        } else if (strcasecmp(state, "waving") == 0 || strcasecmp(state, "jumping") == 0) {
            color = COLOR_STATUS_LISTEN;
        } else if (strcasecmp(state, "waiting") == 0 || strcasecmp(state, "review") == 0) {
            color = COLOR_STATUS_THINK;
        }
        lv_obj_set_style_bg_color(emotion_indicator, color, 0);
    }

    /* 保存情绪 */
    if (emotion) {
        strncpy(current_emotion, emotion, sizeof(current_emotion) - 1);
        current_emotion[sizeof(current_emotion) - 1] = '\0';
    }
}

/**
 * 设置 Wi-Fi 状态显示
 */
void lvgl_chat_ui_set_wifi_status(bool connected) {
    s_wifi_connected = connected;
    if (wifi_status_label) {
        if (connected) {
            lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_status_label, COLOR_STATUS_IDLE, 0);
        } else {
            lv_label_set_text(wifi_status_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_DIM, 0);
        }
    }
    
    if (wifi_page_obj && !lv_obj_has_flag(wifi_page_obj, LV_OBJ_FLAG_HIDDEN)) {
        // Defer updating the whole page to the caller if needed, or rely on boot key toggle to refresh it
        // To avoid circular dependency with wifi_manager in UI file, we will let the main task pass IP.
    }
}

/**
 * 显示/隐藏 Wi-Fi 设置页面
 */
void lvgl_chat_ui_show_wifi_page(bool show, const char *saved_ssid, const char *ip_addr) {
    if (!wifi_page_obj) return;
    if (show) {
        if (saved_ssid) {
            strncpy(s_saved_ssid, saved_ssid, sizeof(s_saved_ssid) - 1);
        }
        lv_obj_clear_flag(wifi_page_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(wifi_page_obj);

        char buf[256];
        if (s_wifi_connected) {
            snprintf(buf, sizeof(buf), "SSID: %s\nStatus: Connected\nIP: %s", 
                     strlen(s_saved_ssid) > 0 ? s_saved_ssid : "Unknown",
                     (ip_addr && strlen(ip_addr) > 0) ? ip_addr : "Pending...");
            if (wifi_info_label) lv_obj_set_style_text_color(wifi_info_label, COLOR_STATUS_IDLE, 0);
        } else {
            snprintf(buf, sizeof(buf), "SSID: %s\nStatus: Disconnected\nIP: None", 
                     strlen(s_saved_ssid) > 0 ? s_saved_ssid : "None");
            if (wifi_info_label) lv_obj_set_style_text_color(wifi_info_label, COLOR_TEXT_DIM, 0);
        }
        if (wifi_info_label) lv_label_set_text(wifi_info_label, buf);
    } else {
        lv_obj_add_flag(wifi_page_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * 清空聊天
 */
void lvgl_chat_ui_clear(void) {
    ESP_LOGI(TAG, "Clearing chat UI");

    /* 清空消息 */
    msg_count = 0;
    msg_head = 0;
    memset(messages, 0, sizeof(messages));

    /* 清空聊天区域 */
    if (chat_area) {
        lv_obj_clean(chat_area);
    }

    /* 重置状态 */
    lvgl_chat_ui_set_status("idle", "idle");
}

/**
 * 显示欢迎界面
 */
void lvgl_chat_ui_welcome(void) {
    ESP_LOGI(TAG, "Showing welcome screen");

    /* 清空聊天区域 */
    if (chat_area) {
        lv_obj_clean(chat_area);
    }

    /* 创建欢迎消息 */
    lv_obj_t *welcome_bubble = lv_obj_create(chat_area);
    lv_obj_set_size(welcome_bubble, LV_HOR_RES - 24, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(welcome_bubble, COLOR_AI_BUB, 0);
    lv_obj_set_style_radius(welcome_bubble, 8, 0);
    obj_set_pad_all(welcome_bubble, 12, 0);
    lv_obj_set_style_border_width(welcome_bubble, 0, 0);
    lv_obj_align(welcome_bubble, LV_ALIGN_TOP_MID, 0, 20);

    /* 欢迎图标（带动画） */
    lv_obj_t *icon = lv_label_create(welcome_bubble);
    lv_label_set_text(icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(icon, COLOR_LABEL_AI, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

    /* 淡入动画 */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, welcome_bubble);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 500);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    /* 欢迎标题 */
    lv_obj_t *title = lv_label_create(welcome_bubble);
    lv_label_set_text(title, "Elara Chat Assistant");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 欢迎信息 */
    lv_obj_t *msg = lv_label_create(welcome_bubble);
    lv_label_set_text(msg, "Hello! I'm Elara, an AI chat assistant.\nHow can I help you?");
    lv_obj_set_style_text_color(msg, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(msg, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, lv_obj_get_width(welcome_bubble) - 24);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 55);

    /* 调整气泡高度 */
    lv_obj_update_layout(welcome_bubble);
    lv_obj_set_height(welcome_bubble, LV_SIZE_CONTENT);

    /* 显示进度条动画 */
    if (progress_bar) {
        lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_bar_set_value(progress_bar, 100, LV_ANIM_ON);
    }

    /* 更新状态 */
    lvgl_chat_ui_set_status("idle", "idle");
}

/**
 * 获取 UI 容器
 */
lv_obj_t *lvgl_chat_ui_get_container(void) {
    return main_container;
}

/**
 * 设置进度条值
 * @param value 进度值 (0-100)
 */
void lvgl_chat_ui_set_progress(int value) {
    if (progress_bar) {
        if (value < 0) {
            /* 隐藏进度条 */
            lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
        } else {
            /* 显示并更新进度 */
            lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(progress_bar, value, LV_ANIM_ON);
        }
    }
}

/**
 * 添加带动画效果的消息气泡
 */
static lv_obj_t *create_animated_message_bubble(MsgRole role, const char *text) {
    lv_obj_t *bubble = create_message_bubble(role, text);

    if (bubble) {
        /* 添加淡入动画 */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bubble);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a, 300);
        lv_anim_set_exec_cb(&a, anim_opa_cb);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }

    return bubble;
}
