#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "ui/lvgl_chat_ui.h"
#include "display/lv_port_disp.h"

static const char *TAG = "WIFI_MGR";
static char s_current_ip[16] = "";

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from Wi-Fi. Retrying...");
        s_current_ip[0] = '\0';
        if (lv_port_disp_lock(-1)) {
            lvgl_chat_ui_set_wifi_status(false);
            lv_port_disp_unlock();
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_current_ip, sizeof(s_current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        if (lv_port_disp_lock(-1)) {
            lvgl_chat_ui_set_wifi_status(true);
            lv_port_disp_unlock();
        }
    }
}

void wifi_manager_init(void) {
    ESP_LOGI(TAG, "Initializing NVS and Wi-Fi...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

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

    /* Read saved Wi-Fi config from NVS */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_cfg", NVS_READONLY, &nvs_handle);
    bool has_config = false;
    if (err == ESP_OK) {
        size_t ssid_len = sizeof(wifi_config.sta.ssid);
        size_t pass_len = sizeof(wifi_config.sta.password);
        if (nvs_get_str(nvs_handle, "ssid", (char *)wifi_config.sta.ssid, &ssid_len) == ESP_OK &&
            nvs_get_str(nvs_handle, "password", (char *)wifi_config.sta.password, &pass_len) == ESP_OK) {
            has_config = true;
            ESP_LOGI(TAG, "Loaded saved Wi-Fi config. SSID: %s", wifi_config.sta.ssid);
        }
        nvs_close(nvs_handle);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (has_config) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
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


