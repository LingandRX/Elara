/**
 * @file lv_port_disp.c
 * LVGL 显示端口 - LVGL v8 版本
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

/* LVGL 显示驱动 */
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;

/* LVGL 互斥锁 */
static SemaphoreHandle_t lvgl_mux = NULL;

/* Boot 模式标志：boot 测试期间跳过 lv_disp_flush_ready */
static volatile bool _boot_mode = false;

/* LVGL 双缓冲（RGB565 格式，2 bytes per pixel） */
void *lv_port_disp_buf1 = NULL;
void *lv_port_disp_buf2 = NULL;

/* 前向声明 */
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void disp_rounder(lv_disp_drv_t *disp_drv, lv_area_t *area);

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

    /* 使用双缓冲，每缓冲 LVGL_BUF_LINES 行 */
    size_t buf_size = LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t);
    lv_port_disp_buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    lv_port_disp_buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!lv_port_disp_buf1 || !lv_port_disp_buf2) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        return;
    }

    /* 初始化显示缓冲区 */
    lv_disp_draw_buf_init(&draw_buf, lv_port_disp_buf1, lv_port_disp_buf2, LCD_WIDTH * LVGL_BUF_LINES);

    /* 初始化显示驱动 */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = disp_flush;
    disp_drv.rounder_cb = disp_rounder;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* 初始化背光 PWM */
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 50000,
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
 * 通知 LVGL 刷新完成（由 SPI on_color_trans_done 回调调用）
 */
void lv_port_disp_flush_ready(void) {
    if (_boot_mode) {
        return; /* boot 测试期间跳过，避免干扰 LVGL 状态 */
    }
    lv_disp_flush_ready(&disp_drv);
}

/**
 * 设置 boot 模式
 */
void lv_port_disp_set_boot_mode(bool boot) {
    _boot_mode = boot;
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
 * Rounder 回调 - 坐标对齐到 2 的倍数
 * LVGL v8 使用 disp_drv.rounder_cb
 */
static void disp_rounder(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    /* 边界保护 */
    if (area->x1 < 0) area->x1 = 0;
    if (area->y1 < 0) area->y1 = 0;
    if (area->x2 >= LCD_WIDTH) area->x2 = LCD_WIDTH - 1;
    if (area->y2 >= LCD_HEIGHT) area->y2 = LCD_HEIGHT - 1;

    /* 起点向下取偶，终点向上取奇，确保宽高为偶数 */
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;

    if (area->x2 >= LCD_WIDTH) area->x2 = LCD_WIDTH - 1;
    if (area->y2 >= LCD_HEIGHT) area->y2 = LCD_HEIGHT - 1;
}

/**
 * 刷新回调 - 将 LVGL 缓冲区数据写入 LCD
 */
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    /* 限制在有效屏幕范围内 */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;

    /* 校验区域合法性 (start 必须小于 end) */
    if (x1 > x2 || y1 > y2) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    /* 使用 esp_lcd API 绘制位图 (x_end = x2 + 1, y_end = y2 + 1) */
    esp_lcd_panel_draw_bitmap(_panel, x1, y1, x2 + 1, y2 + 1, color_p);

    /* 注意：正常情况下不在此处调用 lv_disp_flush_ready，
     * 由 SPI 传输完成回调 (on_color_trans_done) 触发 */
}
