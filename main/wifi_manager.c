#include "wifi_manager.h"
#include "comm/sntp_sync.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";
static char s_current_ip[16] = "";
static int s_retry_count = 0;
static TimerHandle_t s_retry_timer = NULL;
static bool s_wifi_initialized = false;
static bool s_wifi_enabled = false;

/* RSSI 缓存：esp_wifi_sta_get_ap_info 通过事件循环异步返回，
 * 频繁调用会积压事件且可能读到未就绪数据，故节流查询并缓存。 */
static int8_t  s_cached_rssi = -100;
static bool    s_rssi_valid = false;
static bool    s_rssi_logged = false;
static int64_t s_last_rssi_poll_us = 0;
#define RSSI_POLL_INTERVAL_US (2000 * 1000)  /* RSSI 查询节流间隔 2s */

#define WIFI_MAX_RETRY  5
#define WIFI_RETRY_BASE_MS 1000

static void wifi_retry_timer_cb(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "WiFi reconnect attempt %d/%d...", s_retry_count, WIFI_MAX_RETRY);
    esp_wifi_connect();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* e = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Disconnected from Wi-Fi. reason=%d (%s)", e->reason,
                 e->reason == WIFI_REASON_AUTH_FAIL ? "AUTH_FAIL 密码错误" :
                 e->reason == WIFI_REASON_NO_AP_FOUND ? "NO_AP_FOUND 找不到热点" :
                 e->reason == WIFI_REASON_AUTH_EXPIRE ? "AUTH_EXPIRE" :
                 e->reason == WIFI_REASON_HANDSHAKE_TIMEOUT ? "HANDSHAKE_TIMEOUT" :
                 e->reason == WIFI_REASON_ASSOC_FAIL ? "ASSOC_FAIL 关联失败" :
                 e->reason == WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY ? "DISASSOC_DUE_TO_INACTIVITY" : "其他");
        s_current_ip[0] = '\0';

        /* 自动重连（指数退避定时器），避免瞬时失败后不再连接 */
        if (s_retry_count < WIFI_MAX_RETRY) {
            uint32_t delay_ms = WIFI_RETRY_BASE_MS * (1u << s_retry_count);
            if (delay_ms > 8000) delay_ms = 8000;
            s_retry_count++;
            if (!s_retry_timer) {
                s_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(delay_ms),
                                             pdFALSE, NULL, wifi_retry_timer_cb);
            }
            if (s_retry_timer) {
                xTimerChangePeriod(s_retry_timer, pdMS_TO_TICKS(delay_ms), 0);
            }
        } else {
            ESP_LOGW(TAG, "WiFi connect failed after %d retries", WIFI_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_current_ip, sizeof(s_current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        /* 联网成功，同步网络时间 */
        sntp_sync_start();
    }
}

void wifi_manager_init(void) {
    /* esp_wifi_init 只能调用一次，此函数幂等 */
    if (s_wifi_initialized) return;
    s_wifi_initialized = true;

    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    /* 优先使用 NVS 中保存的 Wi-Fi 凭据，未保存时才回退到硬编码 */
    char saved_ssid[64] = {0};
    if (wifi_manager_get_saved_config(saved_ssid, sizeof(saved_ssid))) {
        nvs_handle_t nvs_handle;
        char saved_pass[64] = {0};
        if (nvs_open("wifi_cfg", NVS_READONLY, &nvs_handle) == ESP_OK) {
            size_t len = sizeof(saved_pass);
            nvs_get_str(nvs_handle, "password", saved_pass, &len);
            nvs_close(nvs_handle);
        }
        strncpy((char *)wifi_config.sta.ssid, saved_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, saved_pass, sizeof(wifi_config.sta.password) - 1);
        ESP_LOGI(TAG, "Using saved Wi-Fi config. SSID: %s", saved_ssid);
    } else {
        /* 硬编码 Wi-Fi 凭据（仅作为首次启动回退） */
        strncpy((char *)wifi_config.sta.ssid, "ZTE-6AkyCN", sizeof(wifi_config.sta.ssid));
        strncpy((char *)wifi_config.sta.password, "07200329", sizeof(wifi_config.sta.password));
        ESP_LOGI(TAG, "Using hardcoded Wi-Fi config. SSID: %s", wifi_config.sta.ssid);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    /* 不在此处启动，由 wifi_manager_set_enabled() 根据设置控制 */
}

/**
 * 根据设置启停 Wi-Fi（STA）
 * @param enable true=启动并连接 / false=断开并停止
 */
void wifi_manager_set_enabled(bool enable) {
    if (enable) {
        if (!s_wifi_initialized) {
            wifi_manager_init();
        }
        if (!s_wifi_enabled) {
            esp_err_t err = esp_wifi_start();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(err));
                return;
            }
            s_wifi_enabled = true;
            ESP_LOGI(TAG, "Wi-Fi enabled");
        }
    } else {
        if (s_wifi_enabled) {
            esp_wifi_disconnect();
            esp_wifi_stop();
            s_wifi_enabled = false;
            s_current_ip[0] = '\0';
            s_retry_count = 0;
            ESP_LOGI(TAG, "Wi-Fi disabled");
        }
    }
}

void wifi_manager_set_config(const char *ssid, const char *password) {
    if (!ssid || !password) return;

    ESP_LOGI(TAG, "Saving new Wi-Fi config. SSID: %s", ssid);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_cfg", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS to save Wi-Fi config");
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Disconnect if currently connected, to apply new config
    esp_wifi_disconnect();
    
    // The disconnect event will trigger a reconnect
}

bool wifi_manager_get_saved_config(char *ssid, size_t max_len) {
    if (!ssid || max_len == 0) return false;
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_cfg", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t len = max_len;
        err = nvs_get_str(nvs_handle, "ssid", ssid, &len);
        nvs_close(nvs_handle);
        return (err == ESP_OK);
    }
    return false;
}

bool wifi_manager_get_ip(char *ip_str, size_t max_len) {
    if (!ip_str || max_len == 0) return false;
    if (strlen(s_current_ip) > 0) {
        strncpy(ip_str, s_current_ip, max_len - 1);
        ip_str[max_len - 1] = '\0';
        return true;
    }
    return false;
}

bool wifi_manager_is_connected(void) {
    /* 成功获取 IP 即视为已连接 */
    return s_current_ip[0] != '\0';
}

/* 节流查询 RSSI 并更新缓存 */
static void wifi_manager_poll_rssi(void) {
    if (!wifi_manager_is_connected()) {
        s_rssi_valid = false;
        return;
    }
    int64_t now_us = esp_timer_get_time();
    if (now_us - s_last_rssi_poll_us < RSSI_POLL_INTERVAL_US) {
        return;  /* 节流：距上次查询不足 2s 直接返回缓存 */
    }
    s_last_rssi_poll_us = now_us;

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        s_cached_rssi = ap_info.rssi;
        s_rssi_valid = true;
        if (!s_rssi_logged) {
            s_rssi_logged = true;
            ESP_LOGI(TAG, "RSSI: %d dBm", ap_info.rssi);
        }
    } else if (!s_rssi_logged) {
        s_rssi_logged = true;
        ESP_LOGW(TAG, "esp_wifi_sta_get_ap_info failed: %s", esp_err_to_name(err));
    }
}

bool wifi_manager_get_rssi(int8_t *rssi) {
    if (!rssi) return false;
    wifi_manager_poll_rssi();
    if (s_rssi_valid) {
        *rssi = s_cached_rssi;
        return true;
    }
    return false;
}

int wifi_manager_get_signal_level(void) {
    /* 未连接 → 0 */
    if (!wifi_manager_is_connected()) return 0;
    int8_t rssi = 0;
    /* 已连接但暂无有效 RSSI 缓存（首次查询中/失败）：视为正常连接，避免误显示未连接 */
    if (!wifi_manager_get_rssi(&rssi)) return 3;
    if (rssi >= -60) return 3;   /* 强 */
    if (rssi >= -70) return 2;   /* 中 */
    if (rssi >= -80) return 1;   /* 弱 */
    return 0;                    /* 极弱 */
}


