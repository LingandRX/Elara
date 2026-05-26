#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "display/sh8601.h"
#include "ui/chat_ui.h"
#include "comm/uart_comm.h"

static const char *TAG = "MAIN";

// 全局设备和UI
static sh8601_dev_t lcd;
static ChatUI chatUI;
static QueueHandle_t cmdQueue;

// 命令处理任务
static void cmd_process_task(void *pvParam) {
    UartCmd cmd;
    while (1) {
        if (xQueueReceive(cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (cmd.type) {
                case CMD_STATUS:
                    ESP_LOGI(TAG, "Status: %s", cmd.state);
                    chat_ui_set_status(&chatUI, cmd.state, NULL);
                    chat_ui_redraw(&chatUI);
                    break;

                case CMD_CHAT:
                    ESP_LOGI(TAG, "Chat [%s]: %s", cmd.role, cmd.text);
                    chat_ui_add_msg(&chatUI, cmd.role, cmd.text, cmd.emotion, cmd.chunk);
                    chat_ui_redraw(&chatUI);
                    // LED 根据情绪变化
                    if (strcmp(cmd.emotion, "happy") == 0) {
                        // ws2812_set_all(0, 50, 0); // 绿色
                    } else if (strcmp(cmd.emotion, "sad") == 0) {
                        // ws2812_set_all(0, 0, 50); // 蓝色
                    } else {
                        // ws2812_set_all(50, 50, 0); // 黄色
                    }
                    // ws2812_update();
                    break;

                case CMD_CLEAR:
                    ESP_LOGI(TAG, "Clear chat");
                    chat_ui_clear(&chatUI);
                    chat_ui_welcome(&chatUI);
                    break;

                default:
                    break;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Chat Device Starting...");

    // 初始化 LED
    // ws2812_init();
    // ws2812_set_all(50, 50, 50); // 白色启动指示
    // ws2812_update();

    // 初始化 LCD
    if (!sh8601_init(&lcd)) {
        ESP_LOGE(TAG, "LCD init failed!");
        return;
    }
    sh8601_set_rotation(&lcd, 0); // 竖屏 170x320
    sh8601_fill_screen(&lcd, SH8601_COLOR_BLACK);

    // 初始化 UI
    chat_ui_init(&chatUI, &lcd);

    // 创建命令队列
    cmdQueue = xQueueCreate(16, sizeof(UartCmd));
    if (cmdQueue == NULL) {
        ESP_LOGE(TAG, "Queue create failed!");
        return;
    }

    // 初始化 UART
    if (!uart_comm_init(&cmdQueue)) {
        ESP_LOGE(TAG, "UART init failed!");
        return;
    }

    // 显示欢迎界面
    chat_ui_welcome(&chatUI);

    // 启动 UART 接收任务
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);

    // 启动命令处理任务
    xTaskCreate(cmd_process_task, "cmd_proc", 4096, NULL, 5, NULL);

    // LED 恢复空闲状态
    // ws2812_set_all(0, 20, 0); // 绿色呼吸
    // ws2812_update();

    ESP_LOGI(TAG, "All tasks started. Ready.");

    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
