/**
 * @file lcd_config.h
 * LCD 硬件配置 - 统一管理所有 LCD 相关常量
 */

#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include "driver/gpio.h"

/* LCD 分辨率 */
#define LCD_WIDTH           170
#define LCD_HEIGHT          320

/* LCD SPI 引脚定义 */
#define LCD_PIN_MOSI        GPIO_NUM_13
#define LCD_PIN_SCLK        GPIO_NUM_10
#define LCD_PIN_CS          GPIO_NUM_12
#define LCD_PIN_DC          GPIO_NUM_11
#define LCD_PIN_RST         GPIO_NUM_9
#define LCD_PIN_BL          GPIO_NUM_14

/* SPI 配置 */
#define LCD_SPI_FREQ_HZ     (20 * 1000 * 1000)  // 20MHz
#define LCD_SPI_QUEUE_SIZE  10

/* LVGL 缓冲配置 */
#define LVGL_BUF_LINES      160
#define LVGL_TICK_PERIOD_MS 2

/* BOOT 按键 */
#define BOOT_KEY_PIN        GPIO_NUM_0

/* 背光配置（此开发板低电平亮） */
#define BL_DUTY_ON          0
#define BL_DUTY_OFF         255

/* 背光管理默认配置 */
#define BL_DEFAULT_BRIGHTNESS   125         /* 默认最亮 */
#define BL_SLEEP_TIMEOUT_MS     10000       /* 无操作 10 秒后休眠 */
#define BL_DIM_BRIGHTNESS       0           /* 休眠时亮度，0=直接关闭 */
#define BL_FADE_STEP_MS         20          /* 渐变动效每步间隔 */
#define BL_FADE_STEP_DELTA      8           /* 渐变动效每步亮度变化 */

#endif /* LCD_CONFIG_H */
