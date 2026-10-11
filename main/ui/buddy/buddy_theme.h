/**
 * @file buddy_theme.h
 * Warm 暖棕配色 + 字号/间距规范（见 docs/2026-10-11-ui-redesign-design.md §6）
 */
#ifndef BUDDY_THEME_H
#define BUDDY_THEME_H

#include "lvgl.h"

/* 生成字体的外部声明（同 lv_conf.h 中的约定） */
extern const lv_font_t lv_font_sc_12;
extern const lv_font_t lv_font_sc_14;
extern const lv_font_t lv_font_sc_20;

/* 配色 (Warm) */
#define COLOR_BG        lv_color_hex(0x16110D)  /* 暖黑底 */
#define COLOR_PANEL     lv_color_hex(0x2A211A)  /* 卡片 */
#define COLOR_PANEL2    lv_color_hex(0x372A1F)  /* 选中/高亮 */
#define COLOR_ACCENT    lv_color_hex(0xFFB454)  /* 琥珀主色 */
#define COLOR_TEXT      lv_color_hex(0xFFF6EA)  /* 主文字 */
#define COLOR_TEXT_DIM  lv_color_hex(0x9C7F63)  /* 次要文字 */
#define COLOR_LINE      lv_color_hex(0x3E3023)  /* 分隔线 */
#define COLOR_WARM      lv_color_hex(0xFF8A7A)  /* 脸红/警示 */

/* 状态语义色 */
#define COLOR_GREEN     lv_color_hex(0x00FF00)
#define COLOR_RED       lv_color_hex(0xFF0000)

/* 字号：LVGL 点阵字体固定尺寸，3 档 */
#define FONT_SM         (&lv_font_sc_12)
#define FONT_MD         (&lv_font_sc_14)
#define FONT_LG         (&lv_font_sc_20)

/* 应用层默认字体：Kconfig 的 LVGL 默认字体是 choice，无法指向自定义字体，
 * 故本工程统一以 FONT_MD 作为界面默认字体（见 docs/2026-10-11-ui-redesign-design.md §7）。 */
#define BUDDY_FONT_DEFAULT  FONT_MD

/* 屏幕与布局 */
#define SCREEN_W        170
#define SCREEN_H        320
#define STATUS_BAR_H    24
#define PAGE_PAD_X      13
#define CARD_RADIUS     10
#define ROW_H           20

#endif /* BUDDY_THEME_H */
