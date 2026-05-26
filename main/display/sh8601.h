#ifndef SH8601_H
#define SH8601_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// 引脚定义（来自原理图）
#define SH8601_PIN_MOSI     GPIO_NUM_13
#define SH8601_PIN_SCLK     GPIO_NUM_10
#define SH8601_PIN_CS       GPIO_NUM_12
#define SH8601_PIN_DC       GPIO_NUM_11
#define SH8601_PIN_RST      GPIO_NUM_9
#define SH8601_PIN_BL       GPIO_NUM_14

// 分辨率（竖屏原生尺寸，软件旋转后 320x170）
#define SH8601_WIDTH        170
#define SH8601_HEIGHT       320

// 颜色格式 RGB565
#define SH8601_COLOR_BLACK      0x0000
#define SH8601_COLOR_WHITE      0xFFFF
#define SH8601_COLOR_RED        0xF800
#define SH8601_COLOR_GREEN      0x07E0
#define SH8601_COLOR_BLUE       0x001F
#define SH8601_COLOR_CYAN       0x07FF
#define SH8601_COLOR_MAGENTA    0xF81F
#define SH8601_COLOR_YELLOW     0xFFE0
#define SH8601_COLOR_ORANGE     0xFC00
#define SH8601_COLOR_LIGHTGREY  0xC618

// 颜色混合
#define SH8601_RGB(r, g, b)     ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef struct {
    spi_device_handle_t spi;
    uint16_t *framebuf;     // 帧缓冲 RGB565
    int width;
    int height;
} sh8601_dev_t;

// 初始化
bool sh8601_init(sh8601_dev_t *dev);
void sh8601_set_backlight(bool on);
void sh8601_set_rotation(sh8601_dev_t *dev, uint8_t rot); // 0,1,2,3

// 基础绘图
void sh8601_fill_screen(sh8601_dev_t *dev, uint16_t color);
void sh8601_draw_pixel(sh8601_dev_t *dev, int x, int y, uint16_t color);
void sh8601_draw_rect(sh8601_dev_t *dev, int x, int y, int w, int h, uint16_t color);
void sh8601_draw_bitmap(sh8601_dev_t *dev, int x, int y, int w, int h, const uint8_t *bitmap, uint16_t fg, uint16_t bg);

// 刷新整个屏幕到 LCD
void sh8601_flush(sh8601_dev_t *dev);

// 局部刷新（优化性能）
void sh8601_flush_area(sh8601_dev_t *dev, int x, int y, int w, int h);

#endif
