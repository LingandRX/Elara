#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void wifi_manager_init(void);
void wifi_manager_set_enabled(bool enable);
void wifi_manager_set_config(const char *ssid, const char *password);
bool wifi_manager_get_saved_config(char *ssid, size_t max_len);
bool wifi_manager_get_ip(char *ip_str, size_t max_len);

/**
 * 查询 WiFi 是否已连接（成功获取 IP 即视为已连接）
 * @return true=已连接
 */
bool wifi_manager_is_connected(void);

/**
 * 获取当前连接的信号强度 (RSSI)
 * @param rssi 输出 RSSI 值（dBm，如 -55；越接近 0 信号越强）
 * @return true=获取成功（已连接并关联到 AP）
 */
bool wifi_manager_get_rssi(int8_t *rssi);

/**
 * 获取信号强度等级（供 UI 显示）
 * @return 0=未连接/极弱, 1=弱, 2=中, 3=强
 */
int wifi_manager_get_signal_level(void);

#endif // WIFI_MANAGER_H
