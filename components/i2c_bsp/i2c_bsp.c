#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i2c_bsp.h"
#include "esp_log.h"

#define I2C_MASTER_PORT     I2C_NUM_0
#define I2C_MASTER_SCL_IO   48
#define I2C_MASTER_SDA_IO   47
#define I2C_MASTER_FREQ_HZ  (200 * 1000)
#define I2C_XFER_TIMEOUT_MS 1000

/* 设备句柄表：按 7 位地址缓存，避免对同一地址重复 add_device */
#define MAX_I2C_DEVICES 4

static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev_handles[MAX_I2C_DEVICES];
static uint8_t s_dev_addrs[MAX_I2C_DEVICES];
static int s_dev_count = 0;

/**
 * 获取指定地址的设备句柄，首次访问时自动挂载到总线上
 */
static i2c_master_dev_handle_t get_device(uint8_t addr)
{
  for (int i = 0; i < s_dev_count; i++) {
    if (s_dev_addrs[i] == addr) return s_dev_handles[i];
  }
  if (s_bus_handle == NULL || s_dev_count >= MAX_I2C_DEVICES) return NULL;

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
  };
  if (i2c_master_bus_add_device(s_bus_handle, &dev_cfg, &s_dev_handles[s_dev_count]) != ESP_OK) {
    return NULL;
  }
  s_dev_addrs[s_dev_count] = addr;
  return s_dev_handles[s_dev_count++];
}

void I2C_master_Init(void)
{
  if (s_bus_handle != NULL) return;  // 防止重复初始化

  i2c_master_bus_config_t bus_cfg = {
    .i2c_port = I2C_MASTER_PORT,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus_handle));
}

esp_err_t I2C_writr_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
  i2c_master_dev_handle_t dev = get_device(addr);
  if (dev == NULL) return ESP_ERR_INVALID_STATE;

  /* 拼接 reg + data 为单次传输 */
  uint8_t *pbuf = (uint8_t *)malloc(len + 1);
  if (pbuf == NULL) return ESP_ERR_NO_MEM;
  pbuf[0] = reg;
  memcpy(pbuf + 1, buf, len);

  esp_err_t ret = i2c_master_transmit(dev, pbuf, len + 1, I2C_XFER_TIMEOUT_MS);
  free(pbuf);
  return ret;
}

esp_err_t I2C_read_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
  i2c_master_dev_handle_t dev = get_device(addr);
  if (dev == NULL) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit_receive(dev, &reg, 1, buf, len, I2C_XFER_TIMEOUT_MS);
}

esp_err_t I2C_master_write_read_device(uint8_t addr, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
  i2c_master_dev_handle_t dev = get_device(addr);
  if (dev == NULL) return ESP_ERR_INVALID_STATE;
  return i2c_master_transmit_receive(dev, writeBuf, writeLen, readBuf, readLen, I2C_XFER_TIMEOUT_MS);
}

void i2c_scan(void)
{
  if (s_bus_handle == NULL) {
    ESP_LOGW("i2c_scan", "I2C bus not initialized, call I2C_master_Init() first");
    return;
  }

  int devices_found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    esp_err_t ret = i2c_master_probe(s_bus_handle, address, 100);
    if (ret == ESP_OK) {
      ESP_LOGI("i2c_scan", "I2C device found at address: 0x%02X", address);
      devices_found++;
    } else if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGW("i2c_scan", "I2C timeout at address: 0x%02X", address);
    }
  }
  if (devices_found == 0) {
    ESP_LOGI("i2c_scan", "No I2C devices found");
  } else {
    ESP_LOGI("i2c_scan", "Total I2C devices found: %d", devices_found);
  }
}