#ifndef WS2812_BSP_H
#define WS2812_BSP_H


#define LEDS_COUNT  2 
#define LEDS_PIN    15 

void ws2812_Init(void);
uint32_t ws2812_Wheel(uint8_t pos);
void ws2812_set_pixel(uint8_t id,uint8_t r,uint8_t g,uint8_t b);



#endif