/**
 * @file ble_bridge.h
 * BLE Nordic UART Service 桥接（Bluedroid GATT）
 */

#ifndef BLE_BRIDGE_H
#define BLE_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Nordic UART Service UUIDs */
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

/* 初始化 */
void ble_init(const char *device_name);

/* 状态查询 */
bool     ble_connected(void);
bool     ble_secure(void);
uint32_t ble_passkey(void);
const char* ble_get_device_name(void);

/* 绑定管理 */
void ble_clear_bonds(void);

/* 数据读写 */
size_t ble_available(void);
int    ble_read(void);
size_t ble_write(const uint8_t *data, size_t len);

/* 接收缓冲区大小 */
#define BLE_RX_BUF_SIZE 512

#ifdef __cplusplus
}
#endif

#endif /* BLE_BRIDGE_H */
