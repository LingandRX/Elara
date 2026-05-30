/**
 * @file lv_conf.h
 * LVGL 配置文件 - 匹配官方 ESP32-S3-LCD-1.9 示例
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* 启用配置文件 */
#define LV_CONF_INCLUDE_SIMPLE

/* 颜色深度: 16 位 RGB565 */
#define LV_COLOR_DEPTH 16

/* 字节序交换: 1=大端序（SPI 接口需要） */
#define LV_COLOR_16_SWAP 1

/* 启用透明背景支持 */
#define LV_COLOR_SCREEN_TRANSP 1

/* 屏幕分辨率 */
#define LV_HOR_RES 170
#define LV_VER_RES 320

/* 内存配置: 使用自定义 malloc */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc

/* 使用标准 memcpy/memset */
#define LV_MEMCPY_MEMSET_STD 1

/* 快速内存放入 IRAM */
#define LV_ATTRIBUTE_FAST_MEM_USE_IRAM 1

/* 字体配置 */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* 启用 UTF-8 支持 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* 启用日志 */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#endif /* LV_CONF_H */
