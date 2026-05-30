/**
 * @file lv_conf.h
 * LVGL 配置文件 - 强制覆盖 sdkconfig
 * 必须在 include lvgl.h 之前定义
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* 启用配置文件 */
#define LV_CONF_INCLUDE_SIMPLE

/* 颜色深度: 16 位 RGB565 */
#define LV_COLOR_DEPTH 16

/* 关键: 启用字节序交换，匹配 SH8601 大端序 */
#define LV_COLOR_16_SWAP 1

/* 屏幕分辨率 */
#define LV_HOR_RES 170
#define LV_VER_RES 320

/* 内存配置 */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (32 * 1024)

/* 字体配置 */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* 启用 UTF-8 支持 */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* 启用日志 */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#endif /* LV_CONF_H */
