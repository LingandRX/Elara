#include "SH8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "LCD";

// 软件旋转后的逻辑尺寸
static int _width = SH8601_WIDTH;
static int _height = SH8601_HEIGHT;

// 保存旋转状态，flush 时需要偏移
static uint8_t _rotation = 0;

// 发送命令或数据
static void lcd_send_cmd(sh8601_dev_t *dev, uint8_t cmd) {
    gpio_set_level(SH8601_PIN_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(dev->spi, &t);
}

static void lcd_send_data(sh8601_dev_t *dev, const uint8_t *data, size_t len) {
    gpio_set_level(SH8601_PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(dev->spi, &t);
}

static void lcd_send_byte(sh8601_dev_t *dev, uint8_t data) {
    lcd_send_data(dev, &data, 1);
}

// 设置显示窗口（带偏移补偿）
static void lcd_set_window(sh8601_dev_t *dev, int x, int y, int w, int h) {
    // SH8601 有 35 像素 X 偏移
    int x_offset = (_rotation == 0) ? 35 : 0;
    int y_offset = (_rotation == 1) ? 35 : 0;

    uint16_t xs = x + x_offset;
    uint16_t xe = x + w - 1 + x_offset;
    uint16_t ys = y + y_offset;
    uint16_t ye = y + h - 1 + y_offset;

    uint8_t col[4] = { xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF };
    uint8_t row[4] = { ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF };

    lcd_send_cmd(dev, 0x2A); // CASET
    lcd_send_data(dev, col, 4);
    lcd_send_cmd(dev, 0x2B); // RASET
    lcd_send_data(dev, row, 4);
    lcd_send_cmd(dev, 0x2C); // RAMWR
}

bool sh8601_init(sh8601_dev_t *dev) {
    ESP_LOGI(TAG, "Initializing SH8601...");

    // 分配帧缓冲
    size_t fb_size = SH8601_WIDTH * SH8601_HEIGHT * sizeof(uint16_t);
    dev->framebuf = (uint16_t *)heap_caps_malloc(fb_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!dev->framebuf) {
        ESP_LOGE(TAG, "Frame buffer allocation failed");
        return false;
    }
    memset(dev->framebuf, 0, fb_size);
    dev->width = SH8601_WIDTH;
    dev->height = SH8601_HEIGHT;

    // GPIO 初始化 - 单独配置每个引脚，确保 IO MUX 正确设置
    gpio_reset_pin(SH8601_PIN_RST);
    gpio_set_direction(SH8601_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(SH8601_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(SH8601_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(SH8601_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    gpio_reset_pin(SH8601_PIN_DC);
    gpio_set_direction(SH8601_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(SH8601_PIN_DC, 0);

    // 使用 LEDC PWM 控制背光，避免 GPIO 数字模式冲突
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 20000,  // 20kHz，避免闪烁
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = SH8601_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,  // 100% 亮度（8-bit = 最大值）
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "Backlight LEDC init OK, GPIO%d freq=20kHz", SH8601_PIN_BL);

    // SPI 总线配置 - max_transfer_sz 必须小于硬件限制 (约 32KB)
    #define FLUSH_CHUNK_LINES 40
    spi_bus_config_t buscfg = {
        .mosi_io_num = SH8601_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = SH8601_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SH8601_WIDTH * FLUSH_CHUNK_LINES * 2 + 8,
    };
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000, // 20MHz (官方示例值)
        .mode = 0,
        .spics_io_num = SH8601_PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,
    };
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &dev->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    // SH8601 初始化序列（来自 ESP32-S3-LCD-1.9 官方示例）
    lcd_send_cmd(dev, 0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_send_cmd(dev, 0x36); // MADCTL - Rotate 模式 (后续 set_rotation 会覆盖)
    lcd_send_byte(dev, 0x70);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(dev, 0x3A); // COLMOD - 16bit/pixel RGB565
    lcd_send_byte(dev, 0x55);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(dev, 0xB2);
    lcd_send_byte(dev, 0x0C);
    lcd_send_byte(dev, 0x0C);
    lcd_send_byte(dev, 0x00);
    lcd_send_byte(dev, 0x33);
    lcd_send_byte(dev, 0x33);

    lcd_send_cmd(dev, 0xB7);
    lcd_send_byte(dev, 0x35);

    lcd_send_cmd(dev, 0xBB);
    lcd_send_byte(dev, 0x13);

    lcd_send_cmd(dev, 0xC0);
    lcd_send_byte(dev, 0x2C);

    lcd_send_cmd(dev, 0xC2);
    lcd_send_byte(dev, 0x01);

    lcd_send_cmd(dev, 0xC3);
    lcd_send_byte(dev, 0x0B);

    lcd_send_cmd(dev, 0xC4);
    lcd_send_byte(dev, 0x20);

    lcd_send_cmd(dev, 0xC6);
    lcd_send_byte(dev, 0x0F);

    lcd_send_cmd(dev, 0xD0);
    lcd_send_byte(dev, 0xA4);
    lcd_send_byte(dev, 0xA1);

    lcd_send_cmd(dev, 0xD6);
    lcd_send_byte(dev, 0xA1);

    lcd_send_cmd(dev, 0xE0);
    uint8_t gamma_pos[] = {0x00,0x03,0x07,0x08,0x07,0x15,0x2A,0x44,0x42,0x0A,0x17,0x18,0x25,0x27};
    lcd_send_data(dev, gamma_pos, sizeof(gamma_pos));

    lcd_send_cmd(dev, 0xE1);
    uint8_t gamma_neg[] = {0x00,0x03,0x08,0x07,0x07,0x23,0x2A,0x43,0x42,0x09,0x18,0x17,0x25,0x27};
    lcd_send_data(dev, gamma_neg, sizeof(gamma_neg));

    lcd_send_cmd(dev, 0x21); // INVON
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(dev, 0x13); // NORON
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_send_cmd(dev, 0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_send_cmd(dev, 0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(20));

    // 点亮背光
    sh8601_set_backlight(true);

    ESP_LOGI(TAG, "SH8601 init OK");
    return true;
}

void sh8601_set_backlight(bool on) {
    // 此开发板背光为低电平亮（反相控制），duty=0 最亮，duty=255 最暗
    uint32_t duty = on ? 0 : 255;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ESP_LOGI(TAG, "Backlight set to %s (duty=%lu)", on ? "ON" : "OFF", duty);
}

void sh8601_set_rotation(sh8601_dev_t *dev, uint8_t rot) {
    uint8_t madctl = 0;
    switch (rot % 4) {
        case 0: madctl = 0x00; _width = SH8601_WIDTH; _height = SH8601_HEIGHT; break;
        case 1: madctl = 0x70; _width = SH8601_HEIGHT; _height = SH8601_WIDTH; break; // 横屏 - SH8601 官方值
        case 2: madctl = 0xC0; _width = SH8601_WIDTH; _height = SH8601_HEIGHT; break;
        case 3: madctl = 0xA0; _width = SH8601_HEIGHT; _height = SH8601_WIDTH; break;
    }
    lcd_send_cmd(dev, 0x36);
    lcd_send_byte(dev, madctl);
    dev->width = _width;
    dev->height = _height;
}

void sh8601_fill_screen(sh8601_dev_t *dev, uint16_t color) {
    uint16_t be_color = __builtin_bswap16(color); // SH8601 需要大端序
    for (int i = 0; i < SH8601_WIDTH * SH8601_HEIGHT; i++) {
        dev->framebuf[i] = be_color;
    }
}

void sh8601_draw_pixel(sh8601_dev_t *dev, int x, int y, uint16_t color) {
    if (x < 0 || x >= dev->width || y < 0 || y >= dev->height) return;
    // framebuf 是原生竖屏存储 (170x320)，需要根据旋转映射
    int px, py;
    if (_rotation == 0) {
        // 竖屏：逻辑坐标直接映射到物理坐标
        px = x;
        py = y;
    } else {
        // 横屏：逻辑坐标 (x,y) 旋转映射到物理坐标
        px = y;
        py = SH8601_HEIGHT - 1 - x;
    }
    if (px >= 0 && px < SH8601_WIDTH && py >= 0 && py < SH8601_HEIGHT) {
        dev->framebuf[py * SH8601_WIDTH + px] = __builtin_bswap16(color); // SH8601 需要大端序
    }
}

void sh8601_draw_rect(sh8601_dev_t *dev, int x, int y, int w, int h, uint16_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            sh8601_draw_pixel(dev, x + dx, y + dy, color);
        }
    }
}

void sh8601_draw_bitmap(sh8601_dev_t *dev, int x, int y, int w, int h,
                        const uint8_t *bitmap, uint16_t fg, uint16_t bg) {
    for (int col = 0; col < w; col++) {
        for (int byte_row = 0; byte_row < h / 8; byte_row++) {
            uint8_t byte = bitmap[byte_row * w + col];
            for (int bit = 0; bit < 8; bit++) {
                int row = byte_row * 8 + bit;
                if (row >= h) continue;
                uint16_t color = (byte & (1 << bit)) ? fg : bg;
                sh8601_draw_pixel(dev, x + col, y + row, color);
            }
        }
    }
}

void sh8601_flush(sh8601_dev_t *dev) {
    lcd_set_window(dev, 0, 0, SH8601_WIDTH, SH8601_HEIGHT);
    gpio_set_level(SH8601_PIN_DC, 1);

    // 分块传输，避免超出 SPI 硬件单次传输限制
    for (int y = 0; y < SH8601_HEIGHT; y += FLUSH_CHUNK_LINES) {
        int lines = (y + FLUSH_CHUNK_LINES > SH8601_HEIGHT) ? (SH8601_HEIGHT - y) : FLUSH_CHUNK_LINES;
        size_t len = SH8601_WIDTH * lines * 2;
        spi_transaction_t t = {
            .length = len * 8,
            .tx_buffer = dev->framebuf + y * SH8601_WIDTH,
        };
        spi_device_polling_transmit(dev->spi, &t);
    }
}

void sh8601_flush_area(sh8601_dev_t *dev, int x, int y, int w, int h) {
    // 简化：局部刷新也刷新全屏（SH8601 小屏幕刷新很快）
    sh8601_flush(dev);
}
