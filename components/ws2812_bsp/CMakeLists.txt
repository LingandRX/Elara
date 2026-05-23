idf_component_register(
    SRCS "ws2812_bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES driver espressif__led_strip esp_driver_ledc esp_driver_gpio
)
