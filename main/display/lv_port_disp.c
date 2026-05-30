/**
 * @file lv_port_disp.c
 * LVGL 显示端口 - 匹配官方 ESP32-S3-LCD-1.9 示例
 */

#include "lv_port_disp.h"
#include "lcd_config.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LV_DISP";

/* LCD 面板句柄 */
static esp_lcd_panel_handle_t _panel = NULL;

/* LVGL 互斥锁 */
static SemaphoreHandle_t lvgl_mux = NULL;

/* 前向声明 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**
 * LVGL tick 定时器回调
 */
static void increase_lvgl_tick(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/**
 * 初始化 LVGL 显示端口
 */
void lv_port_disp_init(esp_lcd_panel_handle_t panel) {
    if (!panel) {
        ESP_LOGE(TAG, "LCD panel handle is NULL");
        return;
    }

    _panel = panel;
    ESP_LOGI(TAG, "Initializing LVGL display port...");

    /* 创建显示对象 */
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 使用双缓冲，每缓冲 LVGL_BUF_LINES 行 */
    static lv_color_t *buf1 = NULL;
    static lv_color_t *buf2 = NULL;
    buf1 = heap_caps_malloc(LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        return;
    }
    lv_display_set_buffers(disp, buf1, buf2, LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 初始化背光 PWM */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 20000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = BL_DUTY_OFF,  // 初始关闭
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    /* 创建 LVGL tick 定时器（使用 ESP 定时器，比 FreeRTOS 任务更精确） */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    /* 创建 LVGL 互斥锁（保护多线程访问） */
    lvgl_mux = xSemaphoreCreateMutex();
    if (!lvgl_mux) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    ESP_LOGI(TAG, "LVGL display port initialized: %dx%d (double buffer)", LCD_WIDTH, LCD_HEIGHT);
}

/**
 * 设置背光
 */
void lv_port_disp_set_backlight(bool on) {
    // 此开发板背光为低电平亮（反相控制）
    uint32_t duty = on ? BL_DUTY_ON : BL_DUTY_OFF;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "Backlight set to %s (duty=%lu)", on ? "ON" : "OFF", duty);
}

/**
 * 获取 LVGL 互斥锁
 */
bool lv_port_disp_lock(int timeout_ms) {
    if (!lvgl_mux) {
        ESP_LOGE(TAG, "Mutex not initialized");
        return false;
    }
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

/**
 * 释放 LVGL 互斥锁
 */
void lv_port_disp_unlock(void) {
    if (!lvgl_mux) {
        ESP_LOGE(TAG, "Mutex not initialized");
        return;
    }
    xSemaphoreGive(lvgl_mux);
}

/**
 * 刷新回调 - 将 LVGL 缓冲区数据写入 LCD
 * SH8601 有 SH8601_X_OFFSET 像素的 X 偏移需要补偿
 */
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    /* SH8601 有 X 偏移 */
    int x1 = area->x1 + SH8601_X_OFFSET;
    int x2 = area->x2 + SH8601_X_OFFSET;
    int y1 = area->y1;
    int y2 = area->y2;

    /* 使用 esp_lcd API 绘制位图 */
    esp_lcd_panel_draw_bitmap(_panel, x1, y1, x2 + 1, y2 + 1, px_map);

    /* 通知 LVGL 刷新完成 */
    lv_display_flush_ready(disp);
}
