#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"

#include "display/lcd_config.h"
#include "display/lv_port_disp.h"
#include "input/lvgl_touch.h"
#include "ui/lvgl_chat_ui.h"
#include "comm/uart_comm.h"
#include "wifi_manager.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

/* 全局设备 */
static esp_lcd_panel_handle_t panel = NULL;
static QueueHandle_t cmdQueue;

/* SPI 颜色传输完成回调（通知 LVGL 刷新完成） */
static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_port_disp_flush_ready();
    return false;
}

static bool wifi_page_active = false;

/**
 * BOOT 按键检测任务：循环切换主界面和 Wi-Fi 配置页面
 */
static void boot_key_task(void *pvParam) {
    /* 配置 BOOT 按键 GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_KEY_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Boot key task started, pin=%d", BOOT_KEY_PIN);

    uint8_t last_level = 1;
    while (1) {
        uint8_t level = gpio_get_level(BOOT_KEY_PIN);
        /* 检测下降沿（按下） */
        if (last_level == 1 && level == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); /* 消抖 */
            if (gpio_get_level(BOOT_KEY_PIN) == 0) {
                /* 切换页面状态 */
                wifi_page_active = !wifi_page_active;
                ESP_LOGI(TAG, "Boot key pressed. Wi-Fi page active: %d", wifi_page_active);

                if (lv_port_disp_lock(-1)) {
                    char ssid[64] = {0};
                    char ip_addr[16] = {0};
                    if (wifi_page_active) {
                        wifi_manager_get_saved_config(ssid, sizeof(ssid));
                        wifi_manager_get_ip(ip_addr, sizeof(ip_addr));
                    }
                    lvgl_chat_ui_show_wifi_page(wifi_page_active, ssid, ip_addr);
                    lv_port_disp_unlock();
                }

                /* 等待按键释放 */
                while (gpio_get_level(BOOT_KEY_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }
        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* LVGL 处理任务（使用互斥锁保护，动态延迟） */
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  1

static void lvgl_handler_task(void *pvParam) {
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        if (lv_port_disp_lock(-1)) {
            task_delay_ms = lv_task_handler();
            lv_port_disp_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
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
        .on_color_trans_done = on_color_trans_done,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle));

    /* ST7789V2 面板配置 */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        // .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, 
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));

    /* 复位并初始化面板 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

    /* 设置 X 偏移（170×320 面板在 240×320 控制器中的偏移） */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 35, 0));

    /* 开启显示 */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "LCD init OK");
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Chat Device Starting...");

    /* Initialize NVS and Wi-Fi */
    wifi_manager_init();

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
    xTaskCreate(boot_key_task, "boot_key", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "All tasks started. Ready.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
