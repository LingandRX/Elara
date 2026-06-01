/**
 * @file backlight_manager.c
 * 背光管理器实现 - 亮度调节、自动休眠、渐变动效
 */

#include "backlight_manager.h"
#include "lcd_config.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "BACKLIGHT";

/* 状态变量 */
static uint8_t s_current_brightness = 255;
static uint8_t s_target_brightness = 255;
static uint8_t s_saved_brightness = 255;      /* 休眠前保存的亮度 */
static bool s_auto_sleep_enabled = true;
static uint32_t s_sleep_timeout_ms = BL_SLEEP_TIMEOUT_MS;
static uint8_t s_dim_brightness = BL_DIM_BRIGHTNESS;
static bool s_fade_enabled = true;
static uint32_t s_fade_step_ms = BL_FADE_STEP_MS;
static uint8_t s_fade_step_delta = BL_FADE_STEP_DELTA;
static bool s_is_sleeping = false;
static uint32_t s_last_inactive = 0;

/* FreeRTOS 对象 */
static TimerHandle_t s_fade_timer = NULL;
static SemaphoreHandle_t s_bl_mux = NULL;

/* 前向声明 */
static void fade_timer_cb(TimerHandle_t xTimer);
static void backlight_monitor_task(void *pvParam);

/**
 * 直接应用亮度到 LEDC（无渐变，内部调用，不持锁）
 */
static void bl_apply_raw(uint8_t brightness) {
    uint32_t duty = BL_DUTY_OFF - brightness;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    s_current_brightness = brightness;
}

/**
 * 初始化背光管理器
 */
void backlight_manager_init(uint8_t default_brightness) {
    ESP_LOGI(TAG, "Initializing backlight manager, default=%u", default_brightness);

    s_bl_mux = xSemaphoreCreateMutex();
    if (!s_bl_mux) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    /* 创建渐变动效定时器（单次触发模式，回调中自行控制续跑） */
    s_fade_timer = xTimerCreate("bl_fade", pdMS_TO_TICKS(s_fade_step_ms),
                                pdFALSE, NULL, fade_timer_cb);
    if (!s_fade_timer) {
        ESP_LOGE(TAG, "Failed to create fade timer");
    }

    /* 设置初始亮度 */
    if (default_brightness > 255) default_brightness = 255;
    bl_apply_raw(default_brightness);
    s_target_brightness = default_brightness;
    s_saved_brightness = default_brightness;

    /* 创建背光监控任务 */
    BaseType_t ret = xTaskCreate(backlight_monitor_task, "bl_monitor", 2048, NULL, 1, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
    }

    ESP_LOGI(TAG, "Backlight manager initialized");
}

/**
 * 设置背光亮度
 */
void backlight_set_brightness(uint8_t level) {
    if (!s_bl_mux) return;

    xSemaphoreTake(s_bl_mux, portMAX_DELAY);

    if (level > 255) level = 255;
    s_target_brightness = level;

    if (s_fade_enabled && s_fade_timer) {
        xTimerReset(s_fade_timer, 0);
    } else {
        bl_apply_raw(level);
    }

    xSemaphoreGive(s_bl_mux);
}

/**
 * 开关背光
 */
void backlight_set_on(bool on) {
    backlight_set_brightness(on ? 255 : 0);
}

/**
 * 获取当前亮度
 */
uint8_t backlight_get_brightness(void) {
    return s_current_brightness;
}

/**
 * 配置自动休眠
 */
void backlight_enable_auto_sleep(bool enable, uint32_t timeout_ms, uint8_t dim_brightness) {
    if (!s_bl_mux) return;

    xSemaphoreTake(s_bl_mux, portMAX_DELAY);
    s_auto_sleep_enabled = enable;
    if (timeout_ms > 0) {
        s_sleep_timeout_ms = timeout_ms;
    }
    s_dim_brightness = dim_brightness;
    xSemaphoreGive(s_bl_mux);

    ESP_LOGI(TAG, "Auto sleep %s, timeout=%lu ms, dim=%u",
             enable ? "enabled" : "disabled", s_sleep_timeout_ms, s_dim_brightness);
}

/**
 * 启用/禁用渐变动效
 */
void backlight_set_fade_enabled(bool enable, uint32_t step_ms, uint8_t step_delta) {
    if (!s_bl_mux) return;

    xSemaphoreTake(s_bl_mux, portMAX_DELAY);
    s_fade_enabled = enable;
    if (step_ms > 0) {
        s_fade_step_ms = step_ms;
    }
    if (step_delta > 0) {
        s_fade_step_delta = step_delta;
    }
    xSemaphoreGive(s_bl_mux);

    /* 更新定时器周期 */
    if (s_fade_timer) {
        xTimerChangePeriod(s_fade_timer, pdMS_TO_TICKS(s_fade_step_ms), 0);
    }

    ESP_LOGI(TAG, "Fade %s, step=%lu ms, delta=%u",
             enable ? "enabled" : "disabled", s_fade_step_ms, s_fade_step_delta);
}

/**
 * 获取当前是否处于休眠状态
 */
bool backlight_is_sleeping(void) {
    return s_is_sleeping;
}

/**
 * 获取渐变动效是否启用
 */
bool backlight_is_fade_enabled(void) {
    return s_fade_enabled;
}

/**
 * 渐变动效定时器回调
 * 在定时器服务任务上下文中执行
 */
static void fade_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;

    if (s_current_brightness == s_target_brightness) {
        return;
    }

    int step = (s_target_brightness > s_current_brightness)
                   ? (int)s_fade_step_delta
                   : -(int)s_fade_step_delta;
    int new_val = (int)s_current_brightness + step;

    if (step > 0 && new_val > (int)s_target_brightness) {
        new_val = s_target_brightness;
    }
    if (step < 0 && new_val < (int)s_target_brightness) {
        new_val = s_target_brightness;
    }

    if (s_bl_mux) xSemaphoreTake(s_bl_mux, portMAX_DELAY);
    bl_apply_raw((uint8_t)new_val);
    if (s_bl_mux) xSemaphoreGive(s_bl_mux);

    /* 如果还没达到目标，继续触发定时器 */
    if (new_val != (int)s_target_brightness && s_fade_timer) {
        xTimerReset(s_fade_timer, 0);
    }
}

/**
 * 背光监控任务
 * 每秒检测 LVGL 无操作时间，管理自动休眠/唤醒
 */
static void backlight_monitor_task(void *pvParam) {
    (void)pvParam;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!s_auto_sleep_enabled) {
            s_last_inactive = 0;
            continue;
        }

        uint32_t inactive = lv_disp_get_inactive_time(NULL);

        if (inactive >= s_sleep_timeout_ms && !s_is_sleeping) {
            /* 进入休眠：保存当前亮度并降低/关闭 */
            s_is_sleeping = true;
            s_saved_brightness = s_current_brightness;
            backlight_set_brightness(s_dim_brightness);
            ESP_LOGI(TAG, "Auto sleep: dimmed to %u after %lu ms inactive",
                     s_dim_brightness, inactive);
        } else if (s_is_sleeping && inactive < s_last_inactive) {
            /* inactive 时间下降说明有用户输入，唤醒 */
            s_is_sleeping = false;
            backlight_set_brightness(s_saved_brightness);
            ESP_LOGI(TAG, "Auto sleep: restored to %u", s_saved_brightness);
        }

        s_last_inactive = inactive;
    }
}
