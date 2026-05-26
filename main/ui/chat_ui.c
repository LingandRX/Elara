#include "chat_ui.h"
#include "font/font_hzk.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CHAT_UI";

// ========== 现代配色方案 ==========
#define COLOR_BG           SH8601_RGB(8, 8, 16)      // 深蓝灰背景
#define COLOR_USER_BUB     SH8601_RGB(0, 122, 204)   // 用户蓝色
#define COLOR_AI_BUB       SH8601_RGB(45, 45, 55)    // AI 深灰
#define COLOR_SYS_BUB      SH8601_RGB(70, 70, 80)    // 系统灰
#define COLOR_TEXT         SH8601_COLOR_WHITE
#define COLOR_TEXT_DIM     SH8601_RGB(160, 160, 170) // 暗淡文字
#define COLOR_LABEL_AI     SH8601_RGB(100, 180, 255) // AI 标签色
#define COLOR_LABEL_USER   SH8601_RGB(120, 220, 180) // 用户标签色
#define COLOR_STATUS_IDLE  SH8601_RGB(0, 200, 100)   // 空闲绿
#define COLOR_STATUS_LISTEN SH8601_RGB(255, 200, 0)  // 聆听黄
#define COLOR_STATUS_THINK SH8601_RGB(255, 140, 0)   // 思考橙
#define COLOR_STATUS_REPLY SH8601_RGB(180, 100, 255) // 回复紫
#define COLOR_STATUS_ERR   SH8601_RGB(255, 60, 60)   // 错误红
#define COLOR_DIVIDER      SH8601_RGB(30, 30, 40)    // 分隔线
#define COLOR_STATUS_BG    SH8601_RGB(15, 15, 25)    // 状态栏背景

// ========== 布局 ==========
#define STATUS_H        22
#define BOTTOM_H        20
#define CHAT_Y_START    (STATUS_H + 1)              // +1 分隔线
#define CHAT_Y_END      (320 - BOTTOM_H - 1)        // -1 分隔线
#define CHAT_H          (CHAT_Y_END - CHAT_Y_START)
#define BUBBLE_MAX_W    140                         // 竖屏 170px，留出边距
#define BUBBLE_PAD_X    8
#define BUBBLE_PAD_Y    6
#define MSG_GAP         8                           // 消息间距
#define LABEL_H         10                          // 标签高度

// ========== UTF-8 解码 ==========
static uint16_t decode_utf8(const char **p) {
    uint8_t c = (uint8_t)(**p);
    if (c == 0) return 0;
    if (c < 0x80) { (*p)++; return c; }
    if ((c & 0xE0) == 0xC0) {
        if ((*p)[1] == 0) { (*p)++; return '?'; }
        uint16_t r = ((c & 0x1F) << 6) | ((*p)[1] & 0x3F);
        *p += 2; return r;
    }
    if ((c & 0xF0) == 0xE0) {
        if ((*p)[1] == 0 || (*p)[2] == 0) { (*p)++; return '?'; }
        uint16_t r = ((c & 0x0F) << 12) | (((*p)[1] & 0x3F) << 6) | ((*p)[2] & 0x3F);
        *p += 3; return r;
    }
    (*p)++; return '?';
}

static const FontGlyph* find_glyph(uint16_t unicode) {
    for (int i = 0; i < FONT_GLYPH_COUNT; i++) {
        if (fontTable[i].unicode == unicode) return &fontTable[i];
    }
    return NULL;
}

// 基础字形绘制（无阴影）
static void draw_glyph_raw(sh8601_dev_t *lcd, int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg) {
    const FontGlyph *g = find_glyph(unicode);
    if (!g) return;
    if (!g->bitmap || g->byteLen == 0 || g->byteLen > 64) return;
    sh8601_draw_bitmap(lcd, x, y, g->width, g->height, g->bitmap, fg, bg);
}

// 带阴影的字形绘制（用于气泡内文字，提升可读性）
static void draw_glyph(sh8601_dev_t *lcd, int x, int y, uint16_t unicode, uint16_t fg, uint16_t bg) {
    const FontGlyph *g = find_glyph(unicode);
    if (!g) return;
    if (!g->bitmap || g->byteLen == 0 || g->byteLen > 64) return;
    
    // 阴影颜色：根据背景色计算暗色（40%亮度）
    uint16_t shadow = SH8601_RGB(
        (((bg >> 11) & 0x1F) * 8 * 2) / 5,
        (((bg >> 5) & 0x3F) * 4 * 2) / 5,
        ((bg & 0x1F) * 8 * 2) / 5
    );
    
    // 绘制阴影（右下偏移1px）
    sh8601_draw_bitmap(lcd, x + 1, y + 1, g->width, g->height, g->bitmap, shadow, bg);
    // 绘制主文字
    sh8601_draw_bitmap(lcd, x, y, g->width, g->height, g->bitmap, fg, bg);
}

// ========== 文字工具函数 ==========
static int char_width(uint16_t u) {
    return (u < 0x80) ? 8 : 16;
}

static int text_width(const char *text, int maxW) {
    if (!text) return 0;
    int w = 0, maxLineW = 0;
    const char *p = text;
    while (*p) {
        uint16_t u = decode_utf8(&p);
        if (u == 0) break;
        int cw = char_width(u);
        int spacing = (u < 0x80) ? 1 : 0;
        if (w + cw + spacing > maxW && w > 0) {
            if (w > maxLineW) maxLineW = w;
            w = cw + spacing;
        } else {
            w += cw + spacing;
        }
    }
    if (w > maxLineW) maxLineW = w;
    return maxLineW;
}

static int text_height(const char *text, int maxW) {
    if (!text) return 16;
    int lines = 1, lineW = 0;
    const char *p = text;
    while (*p) {
        uint16_t u = decode_utf8(&p);
        if (u == 0) break;
        int cw = char_width(u);
        int spacing = (u < 0x80) ? 1 : 0;
        if (lineW + cw + spacing > maxW && lineW > 0) { lines++; lineW = cw + spacing; }
        else lineW += cw + spacing;
    }
    return lines * 17;  // 与 draw_wrapped_text 行高同步
}

static int draw_wrapped_text(sh8601_dev_t *lcd, int x, int y, int maxW, const char *text, uint16_t fg, uint16_t bg) {
    if (!text) return 0;
    int cx = x, cy = y, startX = x;
    const char *p = text;
    while (*p) {
        uint16_t u = decode_utf8(&p);
        if (u == 0) break;
        int cw = char_width(u);
        // 字间距：ASCII字符后添加1px间距，让中英文混排更自然
        int spacing = (u < 0x80) ? 1 : 0;
        if (cx + cw + spacing > startX + maxW && cx > startX) { cx = startX; cy += 17; }
        draw_glyph(lcd, cx, cy, u, fg, bg);
        cx += cw + spacing;
    }
    return cy + 16 - y;
}

// ========== 绘制圆角矩形 ==========
static void draw_round_rect(sh8601_dev_t *lcd, int x, int y, int w, int h, uint16_t color) {
    if (w < 4 || h < 4) {
        sh8601_draw_rect(lcd, x, y, w, h, color);
        return;
    }
    // 主体
    sh8601_draw_rect(lcd, x + 2, y, w - 4, h, color);
    sh8601_draw_rect(lcd, x, y + 2, w, h - 4, color);
    // 边缘填充
    sh8601_draw_rect(lcd, x + 1, y + 1, w - 2, 1, color);
    sh8601_draw_rect(lcd, x + 1, y + h - 2, w - 2, 1, color);
    sh8601_draw_rect(lcd, x + 1, y + 1, 1, h - 2, color);
    sh8601_draw_rect(lcd, x + w - 2, y + 1, 1, h - 2, color);
    // 四个圆角像素
    sh8601_draw_pixel(lcd, x + 1, y + 1, color);
    sh8601_draw_pixel(lcd, x + w - 2, y + 1, color);
    sh8601_draw_pixel(lcd, x + 1, y + h - 2, color);
    sh8601_draw_pixel(lcd, x + w - 2, y + h - 2, color);
}

// ========== UI 初始化 ==========
void chat_ui_init(ChatUI *ui, sh8601_dev_t *lcd) {
    memset(ui, 0, sizeof(ChatUI));
    ui->lcd = lcd;
    ui->msgCount = 0;
    ui->msgHead = 0;
    strcpy(ui->statusText, "IDLE");
    ui->statusColor = COLOR_STATUS_IDLE;
}

void chat_ui_add_msg(ChatUI *ui, const char *role, const char *text, const char *emotion, bool chunk) {
    if (!role || !text) return;

    if (chunk && ui->msgCount > 0) {
        int lastIdx = (ui->msgHead + ui->msgCount - 1) % MAX_MSG_COUNT;
        if (strcmp(ui->messages[lastIdx].role, role) == 0 && ui->messages[lastIdx].chunk) {
            strncat(ui->messages[lastIdx].text, text, MSG_TEXT_LEN - strlen(ui->messages[lastIdx].text) - 1);
            ui->messages[lastIdx].chunk = true;
            return;
        }
    }

    int idx = (ui->msgHead + ui->msgCount) % MAX_MSG_COUNT;
    if (ui->msgCount >= MAX_MSG_COUNT) {
        ui->msgHead = (ui->msgHead + 1) % MAX_MSG_COUNT;
    } else {
        ui->msgCount++;
    }
    strncpy(ui->messages[idx].role, role, 7); ui->messages[idx].role[7] = '\0';
    strncpy(ui->messages[idx].text, text, MSG_TEXT_LEN - 1); ui->messages[idx].text[MSG_TEXT_LEN - 1] = '\0';
    strncpy(ui->messages[idx].emotion, emotion ? emotion : "", 7); ui->messages[idx].emotion[7] = '\0';
    ui->messages[idx].chunk = chunk;
}

void chat_ui_set_status(ChatUI *ui, const char *state, const char *emotion) {
    if (!state) return;
    if (strcmp(state, "idle") == 0) {
        strcpy(ui->statusText, "IDLE"); ui->statusColor = COLOR_STATUS_IDLE;
    } else if (strcmp(state, "listening") == 0) {
        strcpy(ui->statusText, "LISTEN"); ui->statusColor = COLOR_STATUS_LISTEN;
    } else if (strcmp(state, "thinking") == 0) {
        strcpy(ui->statusText, "THINK"); ui->statusColor = COLOR_STATUS_THINK;
    } else if (strcmp(state, "responding") == 0) {
        strcpy(ui->statusText, "REPLY"); ui->statusColor = COLOR_STATUS_REPLY;
    } else if (strcmp(state, "error") == 0) {
        strcpy(ui->statusText, "ERR"); ui->statusColor = COLOR_STATUS_ERR;
    }
    if (emotion) {
        strncpy(ui->currentEmotion, emotion, sizeof(ui->currentEmotion) - 1);
        ui->currentEmotion[sizeof(ui->currentEmotion) - 1] = '\0';
    }
}

void chat_ui_clear(ChatUI *ui) {
    ui->msgCount = 0;
    ui->msgHead = 0;
    for (int i = 0; i < MAX_MSG_COUNT; i++) {
        ui->messages[i].role[0] = '\0';
        ui->messages[i].text[0] = '\0';
        ui->messages[i].emotion[0] = '\0';
        ui->messages[i].chunk = false;
    }
}

// ========== 绘制状态栏 ==========
static void draw_status_bar(ChatUI *ui) {
    sh8601_dev_t *lcd = ui->lcd;
    // 背景
    sh8601_draw_rect(lcd, 0, 0, lcd->width, STATUS_H, COLOR_STATUS_BG);
    // 分隔线
    sh8601_draw_rect(lcd, 0, STATUS_H - 1, lcd->width, 1, COLOR_DIVIDER);
    
    // 状态圆点 + 文字
    int x = 6;
    // 状态圆点
    sh8601_draw_pixel(lcd, x, 7, ui->statusColor);
    sh8601_draw_pixel(lcd, x+1, 7, ui->statusColor);
    sh8601_draw_pixel(lcd, x, 8, ui->statusColor);
    sh8601_draw_pixel(lcd, x+1, 8, ui->statusColor);
    x += 10;
    
    const char *p = ui->statusText;
    while (*p) {
        draw_glyph_raw(lcd, x, 3, (uint8_t)(*p), ui->statusColor, COLOR_STATUS_BG);
        x += 8; p++;
    }
    
    // 情绪标签
    if (strlen(ui->currentEmotion) > 0) {
        char emo[24];
        snprintf(emo, sizeof(emo), "[%s]", ui->currentEmotion);
        int w = strlen(emo) * 8;
        x = lcd->width - w - 6;
        p = emo;
        while (*p) {
            draw_glyph_raw(lcd, x, 3, (uint8_t)(*p), COLOR_TEXT_DIM, COLOR_STATUS_BG);
            x += 8; p++;
        }
    }
}

// ========== 绘制底部栏 ==========
static void draw_bottom_bar(ChatUI *ui) {
    sh8601_dev_t *lcd = ui->lcd;
    int y = lcd->height - BOTTOM_H;
    // 分隔线
    sh8601_draw_rect(lcd, 0, y, lcd->width, 1, COLOR_DIVIDER);
    // 背景
    sh8601_draw_rect(lcd, 0, y + 1, lcd->width, BOTTOM_H - 1, COLOR_STATUS_BG);
    
    const char *hint = "Short=Talk  Long=Clr";
    int totalW = strlen(hint) * 8;
    int x = (lcd->width - totalW) / 2;
    const char *p = hint;
    while (*p) {
        draw_glyph_raw(lcd, x, y + 3, (uint8_t)(*p), COLOR_TEXT_DIM, COLOR_STATUS_BG);
        x += 8; p++;
    }
}

// ========== 绘制标签 (AI/You) ==========
static void draw_label(sh8601_dev_t *lcd, int x, int y, const char *role) {
    const char *label = NULL;
    uint16_t color = COLOR_TEXT_DIM;
    if (strcmp(role, "user") == 0) {
        label = "You";
        color = COLOR_LABEL_USER;
    } else if (strcmp(role, "ai") == 0) {
        label = "AI";
        color = COLOR_LABEL_AI;
    } else {
        label = "SYS";
        color = COLOR_TEXT_DIM;
    }
    
    int w = strlen(label) * 8;
    int lx = x;
    if (strcmp(role, "user") == 0) {
        lx = x - w;  // 右对齐
    }
    
    const char *p = label;
    while (*p) {
        draw_glyph_raw(lcd, lx, y, (uint8_t)(*p), color, COLOR_BG);
        lx += 8; p++;
    }
}

// ========== 主重绘函数 ==========
void chat_ui_redraw(ChatUI *ui) {
    sh8601_dev_t *lcd = ui->lcd;
    // 清空聊天区
    sh8601_draw_rect(lcd, 0, CHAT_Y_START, lcd->width, CHAT_H, COLOR_BG);

    int yBottom = CHAT_Y_END;
    
    for (int i = ui->msgCount - 1; i >= 0; i--) {
        int idx = (ui->msgHead + i) % MAX_MSG_COUNT;
        ChatMsg *msg = &ui->messages[idx];

        // 计算气泡尺寸
        int textW = text_width(msg->text, BUBBLE_MAX_W - BUBBLE_PAD_X * 2);
        int textH = text_height(msg->text, BUBBLE_MAX_W - BUBBLE_PAD_X * 2);
        
        // 气泡宽度自适应（最小宽度）
        int bubbleW = textW + BUBBLE_PAD_X * 2;
        if (bubbleW < 40) bubbleW = 40;
        if (bubbleW > BUBBLE_MAX_W) bubbleW = BUBBLE_MAX_W;
        
        int bubbleH = textH + BUBBLE_PAD_Y * 2;
        int totalH = bubbleH + LABEL_H + MSG_GAP;

        yBottom -= totalH;
        if (yBottom < CHAT_Y_START) break;

        // 气泡位置
        int bubbleX;
        uint16_t bubbleColor;
        uint16_t textColor = COLOR_TEXT;
        
        if (strcmp(msg->role, "user") == 0) {
            bubbleX = lcd->width - bubbleW - 6;
            bubbleColor = COLOR_USER_BUB;
        } else if (strcmp(msg->role, "ai") == 0) {
            bubbleX = 6;
            bubbleColor = COLOR_AI_BUB;
        } else {
            bubbleX = (lcd->width - bubbleW) / 2;
            bubbleColor = COLOR_SYS_BUB;
        }

        int bubbleY = yBottom + LABEL_H;

        // 绘制标签
        draw_label(lcd, bubbleX + bubbleW, bubbleY - 9, msg->role);

        // 绘制圆角气泡背景
        draw_round_rect(lcd, bubbleX, bubbleY, bubbleW, bubbleH, bubbleColor);

        // 绘制文字（垂直居中）
        int textY = bubbleY + BUBBLE_PAD_Y + (bubbleH - BUBBLE_PAD_Y * 2 - textH) / 2;
        draw_wrapped_text(lcd, bubbleX + BUBBLE_PAD_X, textY,
                          bubbleW - BUBBLE_PAD_X * 2, msg->text, textColor, bubbleColor);
    }

    draw_status_bar(ui);
    draw_bottom_bar(ui);
    sh8601_flush(lcd);
}

// ========== 欢迎界面 ==========
void chat_ui_welcome(ChatUI *ui) {
    chat_ui_clear(ui);
    chat_ui_add_msg(ui, "system", "System Ready", "", false);
    chat_ui_add_msg(ui, "ai", "Hello! I am AI.", "happy", false);
    chat_ui_set_status(ui, "idle", NULL);
    chat_ui_redraw(ui);
}
