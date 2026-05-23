#include <stdio.h>
#include "user_app.h"

#include "ws2812_bsp.h"

void user_app_init(void);
void example_ws2812_task(void *arg);


void user_top_init(void)
{
  ws2812_Init();
  user_app_init();      //example
}
void user_app_init(void)
{
  xTaskCreate(example_ws2812_task, "example_ws2812_task", 3000, NULL, 2, NULL);
}

void example_ws2812_task(void *arg)
{
  for(;;)
  {
    for (int j = 0; j < 255; j += 2)
    {
      for (int i = 0; i < LEDS_COUNT; i++)
      {
        uint32_t color = ws2812_Wheel((i * 256 / LEDS_COUNT + j) & 255);
        uint8_t color_r = (color >> 16) & 0xFF;
        uint8_t color_g = (color >> 8) & 0xFF;
        uint8_t color_b = color & 0xFF;
        ws2812_set_pixel(i,color_r,color_g,color_b);
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }  
  }
}



