#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"

#include "display/lcd_config.h"
#include "display/lv_port_disp.h"
#include "input/lvgl_touch.h"
#include "ui/lvgl_chat_ui.h"
#include "comm/uart_comm.h"
#include "lvgl.h"

/* SH8601 初始化命令序列（来自官方示例） */
/* 注意: 不包含 0x36 (MADCTL)，由 esp_lcd 驱动基于 rgb_ele_order 自动设置 */
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xb2, (uint8_t []){0x0c, 0x0c, 0x00, 0x33, 0x33}, 5, 0},  // Porch Setting
    {0xb7, (uint8_t []){0x35}, 1, 0},   // Gate Control
    {0xbb, (uint8_t []){0x13}, 1, 0},   // VCOM Setting
    {0xc0, (uint8_t []){0x2c}, 1, 0},   // LCM Control
    {0xc2, (uint8_t []){0x01}, 1, 0},   // VDV and VRH Enable
    {0xc3, (uint8_t []){0x0b}, 1, 0},   // VDV Set
    {0xc4, (uint8_t []){0x20}, 1, 0},   // VCOM Offset Set
    {0xc6, (uint8_t []){0x0f}, 1, 0},   // Frame Rate Control
    {0xd0, (uint8_t []){0xa4, 0xa1}, 2, 0},  // Power Control 1
    {0xd6, (uint8_t []){0xa1}, 1, 0},   // Source Timing Adjust
    {0xe0, (uint8_t []){0x00, 0x03, 0x07, 0x08, 0x07, 0x15, 0x2A, 0x44, 0x42, 0x0A, 0x17, 0x18, 0x25, 0x27}, 14, 0},  // Positive Gamma
    {0xe1, (uint8_t []){0x00, 0x03, 0x08, 0x07, 0x07, 0x23, 0x2A, 0x43, 0x42, 0x09, 0x18, 0x17, 0x25, 0x27}, 14, 0},  // Negative Gamma
    {0x21, (uint8_t []){0x21}, 0, 0},   // INVON - 颜色反转（SH8601 需要）
    {0x11, (uint8_t []){0x11}, 0, 120}, // SLPOUT - 退出睡眠
    {0x29, (uint8_t []){0x29}, 0, 0},   // DISPON - 开启显示
};

static const char *TAG = "MAIN";

/* 全局设备 */
static esp_lcd_panel_handle_t panel = NULL;
static QueueHandle_t cmdQueue;

/* LVGL 处理任务（使用互斥锁保护） */
static void lvgl_handler_task(void *pvParam) {
    while (1) {
        if (lv_port_disp_lock(-1)) {
            lv_timer_handler();
            lv_port_disp_unlock();
        }
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

                case CMD_PROGRESS:
                    ESP_LOGI(TAG, "Progress: %d%%", cmd.progress);
                    lvgl_chat_ui_set_progress(cmd.progress);
                    break;

                default:
                    break;
            }
        }
    }
}

/**
 * 初始化 LCD 面板
 */
static esp_err_t lcd_init(void) {
    ESP_LOGI(TAG, "Initializing LCD...");

    /* SPI 总线配置 */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LVGL_BUF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* LCD Panel IO 配置 */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = LCD_SPI_QUEUE_SIZE,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle));

    /* SH8601 面板配置 */
    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,  // 关键配置：大端序
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel));

    /* 复位并初始化面板 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* 开启显示 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "LCD init OK");
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Chat Device Starting...");

    /* 初始化 LCD 硬件 */
    ESP_ERROR_CHECK(lcd_init());

    /* 初始化 LVGL */
    lv_init();

    /* 初始化 LVGL 显示端口 */
    lv_port_disp_init(panel);

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

    /* 延迟后开启背光，确保屏幕内容已准备好 */
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_port_disp_set_backlight(true);
    ESP_LOGI(TAG, "Backlight ON");

    /* 启动任务 */
    xTaskCreate(lvgl_handler_task, "lvgl_handler", 8192, NULL, 2, NULL);
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_process_task, "cmd_proc", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks started. Ready.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
