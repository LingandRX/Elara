/**
 * @file lvgl_touch.c
 * LVGL 触摸输入驱动适配层实现 - LVGL v8 版本
 */

#include "lvgl_touch.h"
#include "display/lcd_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bsp.h"  /* I2C 驱动初始化 */

/* 引用外部触摸函数 */
extern void touch_Init(void);
extern uint8_t getTouch(uint16_t *x, uint16_t *y);

static const char *TAG = "LVGL_TOUCH";

static lv_indev_t *touch_indev = NULL;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static bool is_pressed = false;
static uint32_t log_counter = 0;

/**
 * LVGL 触摸读取回调
 */
static void touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    uint16_t x, y;
    uint8_t touched = getTouch(&x, &y);

    if (touched) {
        /* CST816T 触摸坐标已直接对应屏幕像素坐标 (0-170, 0-320)
           不需要缩放转换 */
        last_x = x;
        last_y = y;

        /* 坐标裁剪 */
        if (last_x >= LCD_WIDTH) last_x = LCD_WIDTH - 1;
        if (last_y >= LCD_HEIGHT) last_y = LCD_HEIGHT - 1;

        is_pressed = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = last_x;
        data->point.y = last_y;

        /* 限流打印触摸坐标日志，约每 30 帧打印一次 */
        if (++log_counter >= 30) {
            log_counter = 0;
            ESP_LOGI(TAG, "Touch PRESSED: screen=(%u,%u)", last_x, last_y);
        }
    } else {
        if (is_pressed) {
            ESP_LOGI(TAG, "Touch RELEASED: at=(%u,%u)", last_x, last_y);
        }
        is_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
    }
}

/**
 * 初始化 LVGL 触摸输入驱动
 */
void lvgl_touch_init(void) {
    ESP_LOGI(TAG, "Initializing LVGL touch driver...");

    /* 初始化 I2C 驱动（触摸控制器依赖 I2C） */
    ESP_LOGI(TAG, "Initializing I2C driver...");
    I2C_master_Init();

    /* 初始化触摸控制器 */
    touch_Init();

    /* 初始化输入设备驱动 */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    touch_indev = lv_indev_drv_register(&indev_drv);

    if (!touch_indev) {
        ESP_LOGE(TAG, "Failed to register touch input device");
        return;
    }

    ESP_LOGI(TAG, "LVGL touch driver initialized");
}

/**
 * 获取 LVGL 输入设备对象
 */
lv_indev_t *lvgl_get_touch_indev(void) {
    return touch_indev;
}
