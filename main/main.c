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
#include "ui/pet_ui.h"
#include "comm/uart_comm.h"
#include "comm/tcp_server.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

/* 全局设备 */
static esp_lcd_panel_handle_t panel = NULL;
static QueueHandle_t cmdQueue;

/* SPI 颜色传输完成回调 */
static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_port_disp_flush_ready();
    return false;
}

typedef enum {
    PAGE_CHAT,
    PAGE_WIFI,
    PAGE_PETDEX,
    PAGE_MAX
} UIPage;

static UIPage s_current_page = PAGE_CHAT;

/**
 * BOOT 按键检测任务
 */
static void boot_key_task(void *pvParam) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_KEY_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    uint8_t last_level = 1;
    while (1) {
        uint8_t level = gpio_get_level(BOOT_KEY_PIN);
        if (last_level == 1 && level == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (gpio_get_level(BOOT_KEY_PIN) == 0) {
                s_current_page = (s_current_page + 1) % PAGE_MAX;
                if (lv_port_disp_lock(-1)) {
                    lvgl_chat_ui_show_wifi_page(false, NULL, NULL);
                    pet_ui_show(false);
                    if (s_current_page == PAGE_WIFI) {
                        char ssid[64] = {0}, ip_addr[16] = {0};
                        wifi_manager_get_saved_config(ssid, sizeof(ssid));
                        wifi_manager_get_ip(ip_addr, sizeof(ip_addr));
                        lvgl_chat_ui_show_wifi_page(true, ssid, ip_addr);
                    } else if (s_current_page == PAGE_PETDEX) {
                        pet_ui_show(true);
                    }
                    lv_port_disp_unlock();
                }
                while (gpio_get_level(BOOT_KEY_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void lvgl_handler_task(void *pvParam) {
    while (1) {
        uint32_t delay = 500;
        if (lv_port_disp_lock(-1)) {
            delay = lv_task_handler();
            lv_port_disp_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(delay > 500 ? 500 : (delay < 1 ? 1 : delay)));
    }
}

static void cmd_process_task(void *pvParam) {
    UartCmd cmd;
    while (1) {
        if (xQueueReceive(cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (cmd.type) {
                case CMD_STATUS: lvgl_chat_ui_set_status(cmd.state, NULL); break;
                case CMD_CHAT: lvgl_chat_ui_add_msg(cmd.role, cmd.text, cmd.emotion, cmd.chunk); break;
                case CMD_CLEAR: lvgl_chat_ui_clear(); lvgl_chat_ui_welcome(); break;
                case CMD_PROGRESS: lvgl_chat_ui_set_progress(cmd.progress); break;
                case CMD_PETDEX:
                    if (lv_port_disp_lock(-1)) {
                        pet_ui_set_state_by_name(cmd.state);
                        if (s_current_page != PAGE_PETDEX) {
                            s_current_page = PAGE_PETDEX;
                            lvgl_chat_ui_show_wifi_page(false, NULL, NULL);
                            pet_ui_show(true);
                        }
                        lv_port_disp_unlock();
                    }
                    break;
                default: break;
            }
        }
    }
}

static esp_err_t lcd_init(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI, .miso_io_num = -1, .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 160 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC, .cs_gpio_num = LCD_PIN_CS, .pclk_hz = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8, .spi_mode = 0,
        .trans_queue_depth = 10, .on_color_trans_done = on_color_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &io_config, &io_handle));
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 35, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Chat Device Starting...");

    /* 1. 初始化 LCD 硬件 */
    ESP_ERROR_CHECK(lcd_init());

    /* 2. 初始化 LVGL 基础库 */
    lv_init();
    lv_port_disp_init(panel); // 这里会初始化 Mutex
    lvgl_touch_init();

    /* 3. 初始化 UI 界面 */
    lvgl_chat_ui_init();
    pet_ui_init();

    /* 4. 初始化存储 (首次运行可能耗时格式化) */
    storage_init();

    /* 5. 创建命令队列与串口通信模块 */
    cmdQueue = xQueueCreate(16, sizeof(UartCmd));
    uart_comm_init(&cmdQueue);

    /* 6. 初始化 Wi-Fi/esp-netif，再启动依赖 lwIP 的 TCP 服务 */
    wifi_manager_init();
    tcp_server_init();

    lvgl_chat_ui_welcome();
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_port_disp_set_backlight(true);

    xTaskCreate(lvgl_handler_task, "lvgl_handler", 8192, NULL, 2, NULL);
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_process_task, "cmd_proc", 4096, NULL, 5, NULL);
    xTaskCreate(boot_key_task, "boot_key", 4096, NULL, 1, NULL);

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
