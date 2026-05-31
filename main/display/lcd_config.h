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

#endif /* LCD_CONFIG_H */
