/**
 * @file lvgl_touch.c
 * LVGL 触摸输入驱动适配层实现
 */

#include "lvgl_touch.h"
#include "display/sh8601.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 引用外部触摸函数 */
extern void touch_Init(void);
extern uint8_t getTouch(uint16_t *x, uint16_t *y);

static const char *TAG = "LVGL_TOUCH";

static lv_indev_t *touch_indev = NULL;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static bool is_pressed = false;

/**
 * LVGL 触摸读取回调
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    uint8_t touched = getTouch(&x, &y);

    if (touched) {
        /* 坐标映射: 触摸坐标 (0-4095) -> 屏幕坐标 (0-169, 0-319) */
        /* 注意: 可能需要根据实际触摸方向调整 */
        last_x = (uint16_t)((x * SH8601_WIDTH) / 4096);
        last_y = (uint16_t)((y * SH8601_HEIGHT) / 4096);

        /* 坐标裁剪 */
        if (last_x >= SH8601_WIDTH) last_x = SH8601_WIDTH - 1;
        if (last_y >= SH8601_HEIGHT) last_y = SH8601_HEIGHT - 1;

        is_pressed = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = last_x;
        data->point.y = last_y;
    } else {
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

    /* 初始化触摸控制器 */
    touch_Init();

    /* 创建输入设备 */
    touch_indev = lv_indev_create();
    if (!touch_indev) {
        ESP_LOGE(TAG, "Failed to create touch input device");
        return;
    }

    /* 设置输入设备类型为指针 */
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);

    /* 设置读取回调 */
    lv_indev_set_read_cb(touch_indev, touch_read_cb);

    ESP_LOGI(TAG, "LVGL touch driver initialized");
}

/**
 * 获取 LVGL 输入设备对象
 */
lv_indev_t *lvgl_get_touch_indev(void) {
    return touch_indev;
}
