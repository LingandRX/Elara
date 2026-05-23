#include <stdio.h>
#include "ws2812_bsp.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "led_strip.h"

led_strip_handle_t led_strip;
void ws2812_Init(void)
{
  led_strip_config_t strip_config = 
  {
    .strip_gpio_num = LEDS_PIN,  // The GPIO that connected to the LED strip's data line
    .max_leds = LEDS_COUNT,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = 
    {
      .invert_out = false, // don't invert the output signal
    }
  };

  led_strip_rmt_config_t rmt_config = 
  {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)  (64 * 4 / 3)
    .flags = 
    {
      .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
    }
  };

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

uint32_t ws2812_Wheel(uint8_t pos)
{
	uint32_t WheelPos = pos % 0xff;
	if (WheelPos < 85) {
		return ((255 - WheelPos * 3) << 16) | ((WheelPos * 3) << 8);
	}
	if (WheelPos < 170) {
		WheelPos -= 85;
		return (((255 - WheelPos * 3) << 8) | (WheelPos * 3));
	}
	WheelPos -= 170;
	return ((WheelPos * 3) << 16 | (255 - WheelPos * 3));
}

void ws2812_set_pixel(uint8_t id,uint8_t r,uint8_t g,uint8_t b)
{
  led_strip_set_pixel(led_strip,id,r,g,b);
  led_strip_refresh(led_strip);
}