/**
 * @file lv_font_chinese_16.h
 * 中文字体 - 16px
 * 包含常用中文字符和 ASCII 字符
 */

#ifndef LV_FONT_CHINESE_16_H
#define LV_FONT_CHINESE_16_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * 获取中文字体 (16px)
 * @return lv_font_t 指针
 */
const lv_font_t *lv_font_chinese_16_get(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_FONT_CHINESE_16_H */
