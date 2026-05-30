/**
 * @file lv_font_chinese_16.c
 * 中文字体实现 - 16px
 * 使用 LVGL 字体格式
 */

#include "lv_font_chinese_16.h"

/* 字体描述符 */
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    /* ASCII 字符 0x20-0x7E */
    {.bitmap_index = 0, .adv_w = 128, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    /* 可以继续添加更多字符 */
};

/* 字体映射 */
static const uint8_t font_bitmap[] = {
    /* 空格 (0x20) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 可以继续添加更多字形数据 */
};

/* 字体范围 */
static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 0x20,
        .range_length = 95,
        .glyph_id_start = 0,
        .unicode_list = NULL,
        .glyph_id_ofs_list = NULL,
        .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    },
};

/* 字体格式描述 */
static const lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = font_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .cmap_num = 1,
    .bpp = 1,
    .kern_scale = 0,
    .kern_dsc = NULL,
    .kern_classes = 0,
};

/* 字体对象 */
const lv_font_t lv_font_chinese_16 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 16,
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .dsc = &font_dsc,
    .fallback = NULL,
    .user_data = NULL,
};

/**
 * 获取中文字体 (16px)
 */
const lv_font_t *lv_font_chinese_16_get(void) {
    return &lv_font_chinese_16;
}
