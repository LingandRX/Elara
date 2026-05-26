#ifndef CHAT_UI_H
#define CHAT_UI_H

#include "sh8601.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_MSG_COUNT   20
#define MSG_TEXT_LEN    200

// 情绪标签
#define EMOTION_NONE    ""
#define EMOTION_HAPPY   "happy"
#define EMOTION_SAD     "sad"
#define EMOTION_NEUTRAL "neutral"

typedef struct {
    char role[8];       // "user", "ai", "system"
    char text[MSG_TEXT_LEN];
    char emotion[8];
    bool chunk;         // 流式分片
} ChatMsg;

typedef struct {
    sh8601_dev_t *lcd;
    ChatMsg messages[MAX_MSG_COUNT];
    int msgCount;
    int msgHead;
    char currentEmotion[16];
    char statusText[16];
    uint16_t statusColor;
} ChatUI;

// 初始化
void chat_ui_init(ChatUI *ui, sh8601_dev_t *lcd);

// 添加消息
void chat_ui_add_msg(ChatUI *ui, const char *role, const char *text, const char *emotion, bool chunk);

// 设置状态
void chat_ui_set_status(ChatUI *ui, const char *state, const char *emotion);

// 清屏
void chat_ui_clear(ChatUI *ui);

// 重绘全部
void chat_ui_redraw(ChatUI *ui);

// 绘制单条欢迎语（开机用）
void chat_ui_welcome(ChatUI *ui);

#endif
