/**
 * @file lv_port_disp.c
 * LVGL 显示端口 - 基于官方示例
 */

#include "lv_port_disp.h"
#include "sh8601.h"
#include "lvgl.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "LV_DISP";

/* LCD 设备指针 */
static sh8601_dev_t *_lcd = NULL;

/* 前向声明 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**
 * 初始化 LVGL 显示端口
 */
void lv_port_disp_init(sh8601_dev_t *lcd) {
    if (!lcd) {
        ESP_LOGE(TAG, "LCD device is NULL");
        return;
    }

    _lcd = lcd;
    ESP_LOGI(TAG, "Initializing LVGL display port...");

    /* 创建显示对象 */
    lv_display_t *disp = lv_display_create(SH8601_WIDTH, SH8601_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 使用单缓冲，40 行 */
    static lv_color_t buf[SH8601_WIDTH * 40];
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "LVGL display port initialized: %dx%d", SH8601_WIDTH, SH8601_HEIGHT);
}

/**
 * 刷新回调 - 将 LVGL 缓冲区数据写入 LCD
 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    int x = area->x1;
    int y = area->y1;
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;

    /* 设置显示窗口 */
    lcd_set_window(_lcd, x, y, w, h);

    /* 设置 DC=1 准备发送像素数据 */
    gpio_set_level(SH8601_PIN_DC, 1);

    /* 分块发送，每块 40 行 */
    #define FLUSH_LINES 40
    size_t line_bytes = w * 2;
    uint8_t *data = px_map;

    for (int y_off = 0; y_off < h; y_off += FLUSH_LINES) {
        int lines = (y_off + FLUSH_LINES > h) ? (h - y_off) : FLUSH_LINES;
        spi_transaction_t t = {
            .length = lines * line_bytes * 8,
            .tx_buffer = data + y_off * line_bytes,
        };
        spi_device_polling_transmit(_lcd->spi, &t);
    }

    /* 通知 LVGL 刷新完成 */
    lv_display_flush_ready(disp);
}
