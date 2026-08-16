#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============ WiFi 管理器状态机 ============ */
typedef enum {
    WIFI_MANAGER_DISABLED = 0,  /* 总开关关闭（BuddySettings.wifi = false） */
    WIFI_MANAGER_IDLE,          /* 已开启，未连接 */
    WIFI_MANAGER_CONNECTING,    /* 正在连接 */
    WIFI_MANAGER_CONNECTED,     /* 已连接（成功获取 IP） */
    WIFI_MANAGER_FAILED         /* 连接失败（原因见 wifi_manager_get_fail_reason） */
} wifi_manager_state_t;

/* ============ WiFi 列表项（UI 展示用） ============ */
typedef struct {
    char ssid[33];
    int8_t rssi;                /* dBm，越接近 0 越强；未扫描到的已保存网络为 -100 */
    wifi_auth_mode_t auth_mode; /* 认证方式 */
    bool saved;                 /* 凭据已保存到 NVS */
    bool connected;             /* 当前已连接 */
    bool connecting;            /* 正在连接 */
} wifi_item_t;

/* 最多保存的 WiFi 数量（NVS 空间与实用性折中） */
#define WIFI_MANAGER_MAX_SAVED 8
/* 单次提供给 UI 的最大列表条目数 */
#define WIFI_MANAGER_MAX_ITEMS 20

/* ============ 基础接口（保持原有兼容） ============ */
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

/* ============ 状态查询 ============ */

wifi_manager_state_t wifi_manager_get_state(void);

/* 是否正在扫描（扫描为异步，结果通过 wifi_manager_get_items 获取） */
bool wifi_manager_is_scanning(void);

/* WiFi 总开关状态（与 BuddySettings.wifi 同步） */
bool wifi_manager_is_enabled(void);

/**
 * 获取最近一次连接失败的原因（用户可读文案）
 * @param buf 输出缓冲
 * @param max_len 缓冲大小
 */
void wifi_manager_get_fail_reason(char *buf, size_t max_len);

/**
 * 获取当前已连接的 SSID
 * @return true=已连接并写入 buf
 */
bool wifi_manager_get_connected_ssid(char *buf, size_t max_len);

/**
 * 获取状态版本号：状态/扫描结果/保存列表任一变化时递增。
 * UI 层轮询该值即可感知变化（线程安全，无需跨线程调用 LVGL）
 */
uint32_t wifi_manager_get_version(void);

/* ============ 扫描 ============ */

/**
 * 异步发起扫描（不阻塞调用者，可安全在 LVGL 线程调用）。
 * 扫描完成后通过版本号变化感知，用 wifi_manager_get_items 获取结果。
 * @return ESP_OK=已开始 / ESP_ERR_INVALID_STATE=WiFi 关闭或正在连接
 */
esp_err_t wifi_manager_scan_start(void);

/**
 * 获取合并排序后的 WiFi 列表：
 *   当前已连接 → 已保存 → 其他，同组内按 RSSI 从高到低，相同 SSID 去重
 * @param items    输出数组（调用方分配）
 * @param max_items 数组容量
 * @return 实际写入条数
 */
size_t wifi_manager_get_items(wifi_item_t *items, size_t max_items);

/* ============ 连接 ============ */

/**
 * 使用指定密码连接 WiFi（异步，结果通过状态机事件体现）。
 * 连接成功（获取 IP）后自动保存到已保存列表。
 * @return ESP_OK=已发起 / ESP_ERR_INVALID_STATE=WiFi 关闭或参数非法
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * 连接已保存的 WiFi（使用保存的密码，异步）
 * @return ESP_OK=已发起 / ESP_ERR_NOT_FOUND=未保存该网络
 */
esp_err_t wifi_manager_connect_saved(const char *ssid);

/**
 * 主动断开当前连接（不影响已保存列表与扫描缓存）
 */
esp_err_t wifi_manager_disconnect(void);

/* ============ 已保存网络管理 ============ */

int wifi_manager_get_saved_count(void);

/* 按保存顺序（最近优先）读取已保存的 SSID */
bool wifi_manager_get_saved_ssid(int idx, char *buf, size_t max_len);

/**
 * 删除已保存的 WiFi（若当前正连接该网络则先断开）
 */
esp_err_t wifi_manager_remove_saved(const char *ssid);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
