#include <stdio.h>
#include "touch_bsp.h"
#include "i2c_bsp.h"
#include "esp_log.h"

#define I2C_Touch_ADDR 0x15

static const char *TAG = "esp_touch";

void touch_Init(void)
{
  uint8_t data = 0x00;
  esp_err_t ret = I2C_writr_buff(I2C_Touch_ADDR, 0x00, &data, 1); // 切换正常模式
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write reg 0x00 (switch to normal mode): %s", esp_err_to_name(ret));
  }
}

uint8_t getTouch(uint16_t *x, uint16_t *y)
{
  uint8_t tp_temp[7] = {0};
  if (I2C_read_buff(I2C_Touch_ADDR, 0x00, tp_temp, 7) != ESP_OK) {
    return 0; // 读取失败视为无触摸
  }
  uint8_t _num = tp_temp[2];
  if (_num) {
    *x = ((uint16_t)(tp_temp[3] & 0x0f) << 8) + (uint16_t)tp_temp[4];
    *y = ((uint16_t)(tp_temp[5] & 0x0f) << 8) + (uint16_t)tp_temp[6];
    return 1;
  }
  return 0;
}