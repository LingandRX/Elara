/**
 * @file lvgl_chat_ui.h
 * LVGL 聊天界面
 */

#ifndef LVGL_CHAT_UI_H
#define LVGL_CHAT_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 消息角色 */
typedef enum {
    MSG_ROLE_USER,
    MSG_ROLE_AI,
    MSG_ROLE_SYSTEM,
} MsgRole;

/* 消息结构 */
typedef struct {
    MsgRole role;
    char text[256];
    char emotion[16];
    bool is_chunk; /* 流式分片标记 */
} ChatMessage;

/* UI 状态 */
typedef enum {
    UI_STATE_IDLE,
    UI_STATE_LISTENING,
    UI_STATE_THINKING,
    UI_STATE_REPLYING,
    UI_STATE_ERROR,
} UIState;

/**
 * 初始化聊天界面
 */
void lvgl_chat_ui_init(void);

/**
 * 添加消息
 * @param role 角色 (user/ai/system)
 * @param text 消息文本
 * @param emotion 情绪标签
 * @param is_chunk 是否为流式分片
 */
void lvgl_chat_ui_add_msg(const char *role, const char *text, const char *emotion, bool is_chunk);

/**
 * 设置状态
 * @param state 状态文本
 * @param emotion 情绪标签
 */
void lvgl_chat_ui_set_status(const char *state, const char *emotion);

/**
 * 清空聊天
 */
void lvgl_chat_ui_clear(void);

/**
 * 显示欢迎界面
 */
void lvgl_chat_ui_welcome(void);

/**
 * 设置进度条值
 * @param value 进度值 (0-100)，传入负数隐藏进度条
 */
void lvgl_chat_ui_set_progress(int value);

/**
 * 获取 UI 容器
 * @return lv_obj_t 指针
 */
lv_obj_t *lvgl_chat_ui_get_container(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_CHAT_UI_H */
