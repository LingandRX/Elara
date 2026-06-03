/**
 * @file ble_bridge.c
 * BLE Nordic UART Service - Bluedroid GATT 实现
 */

#include "ble_bridge.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "BLE_BRIDGE";

/* Nordic UART Service UUIDs (128-bit) */
static const uint8_t NUS_SERVICE_UUID_128[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
};
static const uint8_t NUS_RX_UUID_128[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
};
static const uint8_t NUS_TX_UUID_128[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e
};

/* 应用 ID */
#define NUS_APP_ID 0x55

/* 特征值句柄 */
static uint16_t nus_handle_table[3];
static uint16_t s_conn_id = 0xFFFF;
static bool s_connected = false;
static bool s_secure = false;
static uint32_t s_passkey = 0;
static uint16_t s_mtu = 23;

/* 接收环形缓冲区 */
static uint8_t  s_rx_buf[BLE_RX_BUF_SIZE];
static volatile size_t s_rx_head = 0;
static volatile size_t s_rx_tail = 0;

/* 设备名称 */
static char s_device_name[32] = "Claude";

/* GATT 属性数据库 */
enum {
    NUS_IDX_SVC,
    NUS_IDX_RX_CHAR,
    NUS_IDX_RX_VAL,
    NUS_IDX_TX_CHAR,
    NUS_IDX_TX_VAL,
    NUS_IDX_TX_CFG,
    NUS_IDX_NB,
};

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint16_t char_cccd_notify = 0x0001;

/* GATT 数据库 */
static const esp_gatts_attr_db_t nus_gatt_db[NUS_IDX_NB] = {
    /* Service Declaration */
    [NUS_IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(NUS_SERVICE_UUID_128), (uint8_t *)NUS_SERVICE_UUID_128}
    },

    /* RX Characteristic Declaration */
    [NUS_IDX_RX_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}
    },

    /* RX Characteristic Value */
    [NUS_IDX_RX_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)NUS_RX_UUID_128,
         ESP_GATT_PERM_WRITE_ENCRYPTED, 512, 0, NULL}
    },

    /* TX Characteristic Declaration */
    [NUS_IDX_TX_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
         CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}
    },

    /* TX Characteristic Value */
    [NUS_IDX_TX_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)NUS_TX_UUID_128,
         ESP_GATT_PERM_READ_ENCRYPTED, 512, 0, NULL}
    },

    /* TX Client Characteristic Configuration Descriptor */
    [NUS_IDX_TX_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid,
         ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
         sizeof(uint16_t), sizeof(char_cccd_notify), (uint8_t *)&char_cccd_notify}
    },
};

/* 将字节推入接收环形缓冲区 */
static void rx_push(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        size_t next = (s_rx_head + 1) % BLE_RX_BUF_SIZE;
        if (next == s_rx_tail) return;  /* 满 */
        s_rx_buf[s_rx_head] = p[i];
        s_rx_head = next;
    }
}

/* GAP 事件处理 */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Pairing success, key type=%d", param->ble_security.auth_cmpl.key_type);
            s_secure = true;
            s_passkey = 0;
        } else {
            ESP_LOGE(TAG, "Pairing failed, reason=0x%x", param->ble_security.auth_cmpl.fail_reason);
            s_secure = false;
        }
        break;
    }
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
        s_passkey = param->ble_security.key_notif.passkey;
        ESP_LOGI(TAG, "Passkey notify: %06lu", (unsigned long)s_passkey);
        break;
    }
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(TAG, "Passkey request");
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT:
        ESP_LOGI(TAG, "OOB request");
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "Numeric comparison request: %lu", (unsigned long)param->ble_security.key_notif.passkey);
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;
    default:
        break;
    }
}

/* GATT 事件处理 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "GATT app registered, app_id=%d", param->reg.app_id);
            esp_ble_gatts_create_attr_tab(nus_gatt_db, gatts_if, NUS_IDX_NB, NUS_APP_ID);
        } else {
            ESP_LOGE(TAG, "GATT register failed, status=%d", param->reg.status);
        }
        break;
    }
    case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
        if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == NUS_IDX_NB) {
            memcpy(nus_handle_table, param->add_attr_tab.handles, sizeof(nus_handle_table));
            esp_ble_gatts_start_service(nus_handle_table[NUS_IDX_SVC]);

            /* 清除所有已有的广播集 */
            esp_ble_gap_ext_adv_set_clear();

            /* 1. 先配置扩展广播参数（必须在设置数据之前） */
            esp_ble_gap_ext_adv_params_t ext_adv_params = {
                .type         = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
                .interval_min = 0x0020,
                .interval_max = 0x0040,
                .channel_map  = ADV_CHNL_ALL,
                .own_addr_type  = BLE_ADDR_TYPE_PUBLIC,
                .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
                .primary_phy    = ESP_BLE_GAP_PHY_1M,
                .secondary_phy  = ESP_BLE_GAP_PHY_1M,
                .sid            = 0,
                .scan_req_notif = false,
            };
            esp_ble_gap_ext_adv_set_params(0, &ext_adv_params);

            /* 2. 构建广播数据（Flags + 128-bit 服务 UUID） */
            uint8_t adv_raw[31];
            size_t  adv_len = 0;
            /* Flags */
            adv_raw[adv_len++] = 0x02;  /* Length */
            adv_raw[adv_len++] = 0x01;  /* Type: Flags */
            adv_raw[adv_len++] = 0x06;  /* General Discr + BR/EDR Not Supported */
            /* 128-bit Service UUID */
            adv_raw[adv_len++] = 17;    /* Length = 1 + 16 */
            adv_raw[adv_len++] = 0x07;  /* Type: Complete List of 128-bit Service UUIDs */
            memcpy(&adv_raw[adv_len], NUS_SERVICE_UUID_128, 16);
            adv_len += 16;

            /* 3. 设置广播数据 */
            esp_ble_gap_config_ext_adv_data_raw(0, (uint16_t)adv_len, adv_raw);

            /* 启动广播 */
            esp_ble_gap_ext_adv_t ext_adv = {
                .instance   = 0,
                .duration   = 0,    /* 无限持续 */
                .max_events = 0,    /* 无限制 */
            };
            esp_ble_gap_ext_adv_start(1, &ext_adv);
        }
        break;
    }
    case ESP_GATTS_CONNECT_EVT: {
        s_conn_id = param->connect.conn_id;
        s_connected = true;
        ESP_LOGI(TAG, "Connected, conn_id=%d", s_conn_id);
        esp_ble_conn_update_params_t conn_params = {
            .bda = {0},
            .min_int = 0x0006,
            .max_int = 0x0010,
            .latency = 0,
            .timeout = 400
        };
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
        s_connected = false;
        s_secure = false;
        s_passkey = 0;
        s_mtu = 23;
        ESP_LOGI(TAG, "Disconnected, reason=0x%x", param->disconnect.reason);
        /* 重启扩展广播 */
        esp_ble_gap_ext_adv_t ext_adv = {
            .instance   = 0,
            .duration   = 0,    /* 无限持续 */
            .max_events = 0,    /* 无限制 */
        };
        esp_ble_gap_ext_adv_start(1, &ext_adv);
        break;
    }
    case ESP_GATTS_MTU_EVT: {
        s_mtu = param->mtu.mtu;
        ESP_LOGI(TAG, "MTU updated: %d", s_mtu);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        if (!param->write.is_prep) {
            if (param->write.handle == nus_handle_table[NUS_IDX_RX_VAL] && param->write.len > 0) {
                rx_push(param->write.value, param->write.len);
            }
        }
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                    ESP_GATT_OK, NULL);
        break;
    }
    default:
        break;
    }
}

/* GATT 接口处理 */
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    gatts_event_handler(event, gatts_if, param);
}

void ble_init(const char *device_name) {
    if (device_name) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }

    /* NVS 已在 app_main 中初始化，此处不再重复 */

    /* 释放经典蓝牙内存，仅保留 BLE */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    /* 初始化 BLE 控制器 */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    /* 初始化 Bluedroid */
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    /* 注册 GAP/GATT 回调 */
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_profile_event_handler));

    /* 注册 GATT 应用 */
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(NUS_APP_ID));

    /* 配置安全 */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    /* 设置设备名称 */
    esp_ble_gap_set_device_name(s_device_name);

    ESP_LOGI(TAG, "BLE initialized, advertising as '%s'", s_device_name);
}

bool ble_connected(void) { return s_connected; }
bool ble_secure(void)    { return s_secure; }
uint32_t ble_passkey(void) { return s_passkey; }

void ble_clear_bonds(void) {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return;
    esp_ble_bond_dev_t *list = (esp_ble_bond_dev_t *)malloc(dev_num * sizeof(esp_ble_bond_dev_t));
    if (!list) return;
    esp_ble_get_bond_device_list(&dev_num, list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(list[i].bd_addr);
    }
    free(list);
    ESP_LOGI(TAG, "Cleared %d bond(s)", dev_num);
}

size_t ble_available(void) {
    return (s_rx_head >= s_rx_tail) ? (s_rx_head - s_rx_tail)
                                   : (BLE_RX_BUF_SIZE - s_rx_tail + s_rx_head);
}

int ble_read(void) {
    if (s_rx_head == s_rx_tail) return -1;
    uint8_t c = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % BLE_RX_BUF_SIZE;
    return c;
}

size_t ble_write(const uint8_t *data, size_t len) {
    if (!s_connected || !data || len == 0) return 0;

    size_t sent = 0;
    while (sent < len) {
        size_t chunk = (len - sent > s_mtu - 3) ? (s_mtu - 3) : (len - sent);
        esp_err_t err = esp_ble_gatts_send_indicate(
            0, s_conn_id, nus_handle_table[NUS_IDX_TX_VAL],
            chunk, (uint8_t *)data + sent, false);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "send_indicate failed: %s", esp_err_to_name(err));
            break;
        }
        sent += chunk;
    }
    return sent;
}
