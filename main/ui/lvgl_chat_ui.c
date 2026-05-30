/**
 * @file lvgl_chat_ui.c
 * LVGL 聊天界面实现
 */

#include "lvgl_chat_ui.h"
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

/* 消息列表 */
static ChatMessage messages[MAX_MESSAGES];
static int msg_count = 0;
static int msg_head = 0;

/* 当前情绪 */
static char current_emotion[16] = "";

/**
 * 创建状态栏
 */
static void create_status_bar(void) {
    status_bar = lv_obj_create(main_container);
    lv_obj_set_size(status_bar, LV_HOR_RES, 28);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 4, 0);
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
    lv_label_set_text(status_label, "空闲");
    lv_obj_set_style_text_color(status_label, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 22, 0);
}

/**
 * 创建聊天区域
 */
static void create_chat_area(void) {
    chat_area = lv_obj_create(main_container);
    lv_obj_set_size(chat_area, LV_HOR_RES, LV_VER_RES - 48); /* 减去状态栏和底部栏 */
    lv_obj_set_style_bg_color(chat_area, COLOR_BG, 0);
    lv_obj_set_style_border_width(chat_area, 0, 0);
    lv_obj_set_style_radius(chat_area, 0, 0);
    lv_obj_set_style_pad_all(chat_area, 4, 0);
    lv_obj_align(chat_area, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_scroll_dir(chat_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(chat_area, LV_SCROLLBAR_MODE_AUTO);
}

/**
 * 创建底部栏
 */
static void create_bottom_bar(void) {
    bottom_bar = lv_obj_create(main_container);
    lv_obj_set_size(bottom_bar, LV_HOR_RES, 20);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x0F0F19), 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(bottom_bar, 2, 0);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    /* 底部文本 */
    lv_obj_t *bottom_label = lv_label_create(bottom_bar);
    lv_label_set_text(bottom_label, "Elara Chat");
    lv_obj_set_style_text_color(bottom_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(bottom_label, &lv_font_montserrat_14, 0);
    lv_obj_align(bottom_label, LV_ALIGN_CENTER, 0, 0);
}

/**
 * 创建消息气泡
 */
static lv_obj_t *create_message_bubble(MsgRole role, const char *text) {
    lv_obj_t *bubble = lv_obj_create(chat_area);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, 8, 0);
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
            lv_label_set_text(role_label, "你");
            lv_obj_set_style_text_color(role_label, COLOR_LABEL_USER, 0);
            break;
        case MSG_ROLE_AI:
            lv_label_set_text(role_label, "AI");
            lv_obj_set_style_text_color(role_label, COLOR_LABEL_AI, 0);
            break;
        case MSG_ROLE_SYSTEM:
            lv_label_set_text(role_label, "系统");
            lv_obj_set_style_text_color(role_label, COLOR_TEXT_DIM, 0);
            break;
    }
    lv_obj_set_style_text_font(role_label, &lv_font_montserrat_14, 0);
    lv_obj_align(role_label, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 消息文本 */
    lv_obj_t *msg_text = lv_label_create(bubble);
    lv_label_set_text(msg_text, text);
    lv_obj_set_style_text_color(msg_text, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(msg_text, &lv_font_montserrat_14, 0);
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
    main_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(main_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(main_container, COLOR_BG, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_radius(main_container, 0, 0);
    lv_obj_set_style_pad_all(main_container, 0, 0);
    lv_obj_align(main_container, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 创建 UI 元素 */
    create_status_bar();
    create_chat_area();
    create_bottom_bar();

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
                uint32_t child_count = lv_obj_get_child_count(chat_area);
                if (child_count > 0) {
                    lv_obj_delete(lv_obj_get_child(chat_area, child_count - 1));
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

    /* 创建消息气泡 */
    create_message_bubble(msg_role, text);

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

    /* 更新情绪指示器 */
    if (emotion_indicator) {
        lv_color_t color;
        if (strcmp(state, "空闲") == 0 || strcmp(state, "idle") == 0) {
            color = COLOR_STATUS_IDLE;
        } else if (strcmp(state, "聆听") == 0 || strcmp(state, "listening") == 0) {
            color = COLOR_STATUS_LISTEN;
        } else if (strcmp(state, "思考") == 0 || strcmp(state, "thinking") == 0) {
            color = COLOR_STATUS_THINK;
        } else if (strcmp(state, "回复") == 0 || strcmp(state, "replying") == 0) {
            color = COLOR_STATUS_REPLY;
        } else if (strcmp(state, "错误") == 0 || strcmp(state, "error") == 0) {
            color = COLOR_STATUS_ERR;
        } else {
            color = COLOR_STATUS_IDLE;
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
    lvgl_chat_ui_set_status("空闲", "idle");
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
    lv_obj_set_style_pad_all(welcome_bubble, 12, 0);
    lv_obj_set_style_border_width(welcome_bubble, 0, 0);
    lv_obj_align(welcome_bubble, LV_ALIGN_TOP_MID, 0, 20);

    /* 欢迎图标 */
    lv_obj_t *icon = lv_label_create(welcome_bubble);
    lv_label_set_text(icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_color(icon, COLOR_LABEL_AI, 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

    /* 欢迎标题 */
    lv_obj_t *title = lv_label_create(welcome_bubble);
    lv_label_set_text(title, "Elara 聊天助手");
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 欢迎信息 */
    lv_obj_t *msg = lv_label_create(welcome_bubble);
    lv_label_set_text(msg, "你好！我是 Elara，一个 AI 聊天助手。\n有什么我可以帮助你的吗？");
    lv_obj_set_style_text_color(msg, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, lv_obj_get_width(welcome_bubble) - 24);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 55);

    /* 调整气泡高度 */
    lv_obj_update_layout(welcome_bubble);
    lv_obj_set_height(welcome_bubble, LV_SIZE_CONTENT);

    /* 更新状态 */
    lvgl_chat_ui_set_status("空闲", "idle");
}

/**
 * 获取 UI 容器
 */
lv_obj_t *lvgl_chat_ui_get_container(void) {
    return main_container;
}
