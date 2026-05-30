#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "display/sh8601.h"
#include "display/lv_port_disp.h"
#include "input/lvgl_touch.h"
#include "ui/lvgl_chat_ui.h"
#include "comm/uart_comm.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

/* 全局设备 */
static sh8601_dev_t lcd;
static QueueHandle_t cmdQueue;

/* LVGL 定时器任务 */
static void lvgl_tick_task(void *pvParam) {
    while (1) {
        lv_tick_inc(1);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* LVGL 处理任务 */
static void lvgl_handler_task(void *pvParam) {
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 命令处理任务 */
static void cmd_process_task(void *pvParam) {
    UartCmd cmd;
    while (1) {
        if (xQueueReceive(cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (cmd.type) {
                case CMD_STATUS:
                    ESP_LOGI(TAG, "Status: %s", cmd.state);
                    lvgl_chat_ui_set_status(cmd.state, NULL);
                    break;

                case CMD_CHAT:
                    ESP_LOGI(TAG, "Chat [%s]: %s", cmd.role, cmd.text);
                    lvgl_chat_ui_add_msg(cmd.role, cmd.text, cmd.emotion, cmd.chunk);
                    break;

                case CMD_CLEAR:
                    ESP_LOGI(TAG, "Clear chat");
                    lvgl_chat_ui_clear();
                    lvgl_chat_ui_welcome();
                    break;

                default:
                    break;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Chat Device Starting...");

    /* 初始化 LCD 硬件 */
    if (!sh8601_init(&lcd)) {
        ESP_LOGE(TAG, "LCD init failed!");
        return;
    }
    sh8601_set_rotation(&lcd, 0);

    /* 初始化 LVGL */
    lv_init();

    /* 初始化 LVGL 显示端口 */
    lv_port_disp_init(&lcd);

    /* 初始化 LVGL 触摸端口 */
    lvgl_touch_init();

    /* 初始化聊天界面 */
    lvgl_chat_ui_init();

    /* 创建命令队列 */
    cmdQueue = xQueueCreate(16, sizeof(UartCmd));
    if (cmdQueue == NULL) {
        ESP_LOGE(TAG, "Queue create failed!");
        return;
    }

    /* 初始化 UART */
    if (!uart_comm_init(&cmdQueue)) {
        ESP_LOGE(TAG, "UART init failed!");
        return;
    }

    /* 显示欢迎界面 */
    lvgl_chat_ui_welcome();

    /* 启动任务 */
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 6, NULL);
    xTaskCreate(lvgl_handler_task, "lvgl_handler", 4096, NULL, 5, NULL);
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_process_task, "cmd_proc", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks started. Ready.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
