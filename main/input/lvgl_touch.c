/**
 * @file lvgl_touch.c
 * LVGL 触摸输入驱动适配层实现 - LVGL v8 版本
 */

#include "lvgl_touch.h"
#include "display/lcd_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bsp.h"
#include <stdlib.h>

extern void touch_Init(void);
extern uint8_t getTouch(uint16_t *x, uint16_t *y);

static const char *TAG = "LVGL_TOUCH";

static lv_indev_t *touch_indev = NULL;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static bool is_pressed = false;

/* CST816T 滑动时触点数寄存器会瞬时丢点, 需防抖保持一次连续按压:
 * 点击用短防抖, 有位移的滑动用长防抖桥接丢点. */
#define TAP_DEBOUNCE     2   /* 点击: 连续 2 次无触点即松开 */
#define SWIPE_DEBOUNCE   8   /* 滑动: 连续 8 次无触点(~260ms)才松开 */
#define SWIPE_THRESHOLD  25  /* 位移超过该值判定为滑动 */

static uint8_t release_streak = 0;
static uint16_t press_x = 0;
static uint16_t press_y = 0;
static bool swiping = false;

static lvgl_touch_swipe_cb_t swipe_cb = NULL;

void lvgl_touch_set_swipe_cb(lvgl_touch_swipe_cb_t cb) {
    swipe_cb = cb;
}

/**
 * LVGL 触摸读取回调
 */
static void touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    uint16_t x, y;
    bool touched = getTouch(&x, &y);

    if (touched) {
        release_streak = 0;
        if (!is_pressed) {
            press_x = x;
            press_y = y;
            swiping = false;
        } else if (!swiping &&
                   (abs((int)x - press_x) > SWIPE_THRESHOLD ||
                    abs((int)y - press_y) > SWIPE_THRESHOLD)) {
            swiping = true;
        }
        last_x = x;
        last_y = y;
        if (last_x >= LCD_WIDTH)  last_x = LCD_WIDTH - 1;
        if (last_y >= LCD_HEIGHT) last_y = LCD_HEIGHT - 1;
        is_pressed = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = last_x;
        data->point.y = last_y;
    } else if (is_pressed) {
        if (++release_streak < (swiping ? SWIPE_DEBOUNCE : TAP_DEBOUNCE)) {
            /* 瞬时丢点: 保持按下, 沿用上一位置平滑抖动 */
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = last_x;
            data->point.y = last_y;
        } else {
            /* 松开: 若为滑动则按净位移方向回调 */
            if (swiping && swipe_cb) {
                int dx = (int)last_x - (int)press_x;
                int dy = (int)last_y - (int)press_y;
                lv_dir_t dir;
                if (abs(dx) >= abs(dy)) dir = (dx > 0) ? LV_DIR_RIGHT : LV_DIR_LEFT;
                else                    dir = (dy > 0) ? LV_DIR_BOTTOM : LV_DIR_TOP;
                swipe_cb(dir);
            }
            is_pressed = false;
            release_streak = 0;
            swiping = false;
            data->state = LV_INDEV_STATE_RELEASED;
            data->point.x = last_x;
            data->point.y = last_y;
        }
    } else {
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

    I2C_master_Init();
    touch_Init();

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
