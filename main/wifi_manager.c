#include "wifi_manager.h"
#include "comm/sntp_sync.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "WIFI_MGR";

/* ============ 常量 ============ */
#define WIFI_MAX_RETRY        5      /* 掉线重连次数上限（保持原有行为） */
#define WIFI_RETRY_BASE_MS    1000   /* 掉线重连指数退避基数 */
#define WIFI_CONNECT_RETRY    2      /* 单网络连接尝试次数上限 */
#define WIFI_CONNECT_TIMEOUT_MS 15000 /* 连接超时（含 DHCP 取 IP） */
#define WIFI_SCAN_MAX_AP      32     /* 驱动层最多读取的原始扫描记录数 */
#define WIFI_PWD_MAX          64     /* ESP32 WiFi 密码最大长度 */

/* ============ 运行状态 ============ */
static bool s_wifi_initialized = false;
static bool s_wifi_enabled = false;
static char s_current_ip[16] = "";

/* 互斥锁：保护 状态机/保存列表/扫描缓存（事件任务、LVGL 任务、UART 任务并发访问） */
static SemaphoreHandle_t s_lock = NULL;

/* 状态机与 UI 版本号 */
static wifi_manager_state_t s_state = WIFI_MANAGER_DISABLED;
static bool     s_scanning = false;
static uint32_t s_version = 1;
static char     s_fail_reason[32] = "";

/* 当前/正在连接的网络（密码仅内部使用，禁止输出到日志） */
static char s_connected_ssid[33] = "";
static char s_connecting_ssid[33] = "";
static char s_connecting_pwd[WIFI_PWD_MAX] = "";
static wifi_auth_mode_t s_connecting_auth = WIFI_AUTH_WPA2_PSK;
static uint8_t s_connect_attempts = 0;
static bool s_expect_disconnect = false;  /* 主动断开（切网/超时/停用），不算失败 */
static bool s_auto_connecting = false;    /* 启动自动连接流程进行中 */
static bool s_switch_pending = false;     /* 切换网络：等待断开事件后续接连接 */

/* 自动连接候选列表（SSID 快照，可见者按 RSSI 降序，不可见的追加尾部） */
static char s_auto_cands[WIFI_MANAGER_MAX_SAVED][33];
static int  s_auto_idx = 0;
static int  s_auto_count = 0;

/* ============ 已保存网络（NVS namespace "wifinet"） ============ */
typedef struct {
    uint32_t magic;                 /* 结构校验魔数 */
    char     ssid[33];
    char     password[WIFI_PWD_MAX];
    uint8_t  auth_mode;
} wifi_saved_net_t;

#define WIFI_SAVED_MAGIC 0x574E4554u  /* 'WNET' */
static wifi_saved_net_t s_saved[WIFI_MANAGER_MAX_SAVED];
static int s_saved_count = 0;

/* ============ 扫描缓存（按 SSID 去重后） ============ */
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t auth;
} scan_rec_t;
static scan_rec_t s_scan[WIFI_MANAGER_MAX_ITEMS];
static int s_scan_count = 0;
/* 驱动层原始记录的静态缓冲（事件任务上下文使用，避免堆分配失败路径） */
static wifi_ap_record_t s_scan_raw[WIFI_SCAN_MAX_AP];

/* ============ 定时器 ============ */
static TimerHandle_t s_retry_timer = NULL;        /* 连接/重连（指数退避） */
static TimerHandle_t s_connect_timeout = NULL;    /* 连接超时（含 DHCP） */
static int s_retry_count = 0;

/* RSSI 缓存：esp_wifi_sta_get_ap_info 通过事件循环异步返回，
 * 频繁调用会积压事件且可能读到未就绪数据，故节流查询并缓存。 */
static int8_t  s_cached_rssi = -100;
static bool    s_rssi_valid = false;
static int64_t s_last_rssi_poll_us = 0;
#define RSSI_POLL_INTERVAL_US (2000 * 1000)  /* RSSI 查询节流间隔 2s */

/* ============ 前向声明 ============ */
static esp_err_t start_connect(const char *ssid, const char *password, bool auto_mode);
static void auto_connect_next(void);

/* ============ 内部工具 ============ */

static void version_bump(void) {
    /* 调用方需已持有 s_lock */
    s_version++;
}

static void set_state_locked(wifi_manager_state_t st) {
    if (s_state != st) {
        s_state = st;
        version_bump();
    }
}

static void lock_take(void) {
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void lock_give(void) {
    if (s_lock) xSemaphoreGive(s_lock);
}

/* ============ 已保存网络：NVS 持久化 ============ */

static void saved_store_nvs(void) {
    /* 调用方需已持有 s_lock */
    nvs_handle_t h;
    if (nvs_open("wifinet", NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace wifinet (write)");
        return;
    }
    nvs_set_u8(h, "cnt", (uint8_t)s_saved_count);
    for (int i = 0; i < s_saved_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "net%d", i);
        nvs_set_blob(h, key, &s_saved[i], sizeof(wifi_saved_net_t));
    }
    /* 清除多余旧条目（保存数量减少时） */
    for (int i = s_saved_count; i < WIFI_MANAGER_MAX_SAVED; i++) {
        char key[16];
        snprintf(key, sizeof(key), "net%d", i);
        nvs_erase_key(h, key);
    }
    nvs_commit(h);
    nvs_close(h);
}

static int saved_find(const char *ssid) {
    for (int i = 0; i < s_saved_count; i++) {
        if (strcmp(s_saved[i].ssid, ssid) == 0) return i;
    }
    return -1;
}

static void saved_add(const char *ssid, const char *password, wifi_auth_mode_t auth) {
    /* 调用方需已持有 s_lock */
    if (!ssid || !ssid[0]) return;

    int idx = saved_find(ssid);
    if (idx >= 0) {
        /* 已存在：更新凭据并移到最前（最近使用优先） */
        wifi_saved_net_t rec = s_saved[idx];
        strncpy(rec.password, password ? password : "", WIFI_PWD_MAX - 1);
        rec.password[WIFI_PWD_MAX - 1] = '\0';
        rec.auth_mode = (uint8_t)auth;
        memmove(&s_saved[1], &s_saved[0], idx * sizeof(wifi_saved_net_t));
        s_saved[0] = rec;
    } else {
        /* 新网络：插入头部；满则淘汰最旧的一条 */
        if (s_saved_count >= WIFI_MANAGER_MAX_SAVED) {
            s_saved_count = WIFI_MANAGER_MAX_SAVED - 1;
        }
        memmove(&s_saved[1], &s_saved[0], s_saved_count * sizeof(wifi_saved_net_t));
        s_saved_count++;
        memset(&s_saved[0], 0, sizeof(wifi_saved_net_t));
        s_saved[0].magic = WIFI_SAVED_MAGIC;
        strncpy(s_saved[0].ssid, ssid, sizeof(s_saved[0].ssid) - 1);
        strncpy(s_saved[0].password, password ? password : "", WIFI_PWD_MAX - 1);
        s_saved[0].auth_mode = (uint8_t)auth;
    }
    saved_store_nvs();
    version_bump();
}

static void saved_remove(const char *ssid) {
    /* 调用方需已持有 s_lock */
    int idx = saved_find(ssid);
    if (idx < 0) return;
    memmove(&s_saved[idx], &s_saved[idx + 1],
            (s_saved_count - idx - 1) * sizeof(wifi_saved_net_t));
    s_saved_count--;
    saved_store_nvs();
    version_bump();
}

/**
 * 加载已保存网络列表。
 * - 首次使用（wifinet 无数据）时从旧版单网络配置 "wifi_cfg" 迁移；
 * - 完全无配置时回退到硬编码凭据（保持原开箱即连行为）；
 * - NVS 数据损坏时跳过坏条目，不会崩溃。
 */
static void saved_load(void) {
    lock_take();
    s_saved_count = 0;

    nvs_handle_t h;
    bool loaded = false;
    if (nvs_open("wifinet", NVS_READONLY, &h) == ESP_OK) {
        uint8_t cnt = 0;
        if (nvs_get_u8(h, "cnt", &cnt) == ESP_OK) {
            if (cnt > WIFI_MANAGER_MAX_SAVED) cnt = WIFI_MANAGER_MAX_SAVED;
            for (int i = 0; i < cnt; i++) {
                char key[16];
                snprintf(key, sizeof(key), "net%d", i);
                wifi_saved_net_t rec;
                size_t len = sizeof(rec);
                if (nvs_get_blob(h, key, &rec, &len) != ESP_OK ||
                    len != sizeof(rec) || rec.magic != WIFI_SAVED_MAGIC) {
                    ESP_LOGW(TAG, "Saved network %d corrupt, skipped", i);
                    continue;
                }
                rec.ssid[sizeof(rec.ssid) - 1] = '\0';
                rec.password[sizeof(rec.password) - 1] = '\0';
                if (!rec.ssid[0]) continue;
                if (s_saved_count < WIFI_MANAGER_MAX_SAVED) {
                    s_saved[s_saved_count++] = rec;
                }
            }
        }
        nvs_close(h);
        loaded = (s_saved_count > 0);
    }

    if (!loaded) {
        /* 从旧版 wifi_cfg（单网络）迁移，保留用户现有配置 */
        char legacy_ssid[64] = {0};
        char legacy_pwd[64] = {0};
        bool have_legacy = false;
        nvs_handle_t lh;
        if (nvs_open("wifi_cfg", NVS_READONLY, &lh) == ESP_OK) {
            size_t len = sizeof(legacy_ssid);
            if (nvs_get_str(lh, "ssid", legacy_ssid, &len) == ESP_OK && legacy_ssid[0]) {
                len = sizeof(legacy_pwd);
                if (nvs_get_str(lh, "password", legacy_pwd, &len) != ESP_OK) {
                    legacy_pwd[0] = '\0';
                }
                have_legacy = true;
            }
            nvs_close(lh);
        }
        if (have_legacy) {
            ESP_LOGI(TAG, "Migrating legacy WiFi config to multi-network store");
            saved_add(legacy_ssid, legacy_pwd, WIFI_AUTH_WPA2_PSK);
        } else {
            /* 首次烧写且无任何配置：回退硬编码凭据（保持原有行为） */
            ESP_LOGI(TAG, "No saved WiFi, seeding factory default");
            saved_add("ZTE-6AkyCN", "07200329", WIFI_AUTH_WPA2_PSK);
        }
    }

    ESP_LOGI(TAG, "Saved networks: %d", s_saved_count);
    lock_give();
}

/* ============ 连接超时定时器 ============ */

static void connect_timeout_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    ESP_LOGW(TAG, "Connect timeout (%d ms), disconnecting", WIFI_CONNECT_TIMEOUT_MS);

    lock_take();
    bool connecting = (s_state == WIFI_MANAGER_CONNECTING);
    if (connecting) {
        strncpy(s_fail_reason, "连接超时", sizeof(s_fail_reason) - 1);
        s_fail_reason[sizeof(s_fail_reason) - 1] = '\0';
        s_expect_disconnect = true;
    }
    lock_give();

    if (connecting) {
        esp_wifi_disconnect(); /* 断开事件走失败上报路径 */
    }
}

static void connect_timeout_start(void) {
    if (!s_connect_timeout) {
        s_connect_timeout = xTimerCreate("wifi_cto", pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS),
                                         pdFALSE, NULL, connect_timeout_cb);
    }
    if (s_connect_timeout) {
        xTimerStop(s_connect_timeout, 0);
        xTimerChangePeriod(s_connect_timeout, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS), 0);
        xTimerStart(s_connect_timeout, 0);
    }
}

static void connect_timeout_stop(void) {
    if (s_connect_timeout) xTimerStop(s_connect_timeout, 0);
}

/* ============ 连接重试定时器 ============ */

static void wifi_retry_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    ESP_LOGI(TAG, "WiFi connect retry...");
    lock_take();
    set_state_locked(WIFI_MANAGER_CONNECTING); /* s_connecting_ssid 保持为目标网络 */
    lock_give();
    connect_timeout_start();
    esp_wifi_connect();
}

/* ============ 失败原因映射（用户可读文案） ============ */

static const char *reason_to_text(int reason) {
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
        return "密码错误或认证失败";
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return "找不到该网络";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_TIMEOUT:
        return "连接超时";
    case WIFI_REASON_ASSOC_FAIL:
        return "关联失败";
    case WIFI_REASON_CONNECTION_FAIL:
        return "连接失败";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "信号不稳定";
    default:
        return "未知错误";
    }
}

/* ============ 自动连接 ============ */

/* 尝试下一个自动连接候选；全部失败则回到 IDLE（不阻塞系统） */
static void auto_connect_next(void) {
    char ssid[33] = "";
    char pwd[WIFI_PWD_MAX] = "";

    lock_take();
    while (s_auto_idx < s_auto_count && s_auto_idx < WIFI_MANAGER_MAX_SAVED) {
        const char *cand = s_auto_cands[s_auto_idx++];
        int idx = saved_find(cand);
        if (idx >= 0) {
            strncpy(ssid, s_saved[idx].ssid, sizeof(ssid) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
            strncpy(pwd, s_saved[idx].password, sizeof(pwd) - 1);
            pwd[sizeof(pwd) - 1] = '\0';
            break;
        }
    }
    if (ssid[0] == '\0') {
        s_auto_connecting = false;
        set_state_locked(WIFI_MANAGER_IDLE);
        ESP_LOGW(TAG, "Auto-connect: all saved networks failed");
        lock_give();
        return;
    }
    lock_give();

    ESP_LOGI(TAG, "Auto-connect trying saved network: %s", ssid);
    start_connect(ssid, pwd, true);
}

/* 扫描完成后：信号最强的已保存网络优先，不可见的追加候选尾部 */
static void auto_connect_build_candidates(void) {
    /* 调用方需已持有 s_lock */
    s_auto_count = 0;
    s_auto_idx = 0;

    /* 已保存 ∩ 扫描可见（s_scan 已按 RSSI 降序） */
    for (int i = 0; i < s_scan_count && s_auto_count < WIFI_MANAGER_MAX_SAVED; i++) {
        if (saved_find(s_scan[i].ssid) >= 0) {
            strncpy(s_auto_cands[s_auto_count], s_scan[i].ssid,
                    sizeof(s_auto_cands[0]) - 1);
            s_auto_cands[s_auto_count][sizeof(s_auto_cands[0]) - 1] = '\0';
            s_auto_count++;
        }
    }
    /* 扫描不可见的已保存网络（含隐藏 SSID）追加在候选尾部 */
    for (int i = 0; i < s_saved_count && s_auto_count < WIFI_MANAGER_MAX_SAVED; i++) {
        bool dup = false;
        for (int j = 0; j < s_auto_count; j++) {
            if (strcmp(s_auto_cands[j], s_saved[i].ssid) == 0) { dup = true; break; }
        }
        if (!dup) {
            strncpy(s_auto_cands[s_auto_count], s_saved[i].ssid,
                    sizeof(s_auto_cands[0]) - 1);
            s_auto_cands[s_auto_count][sizeof(s_auto_cands[0]) - 1] = '\0';
            s_auto_count++;
        }
    }
}

static void auto_connect_begin(void) {
    lock_take();
    if (s_saved_count == 0) {
        set_state_locked(WIFI_MANAGER_IDLE);
        lock_give();
        return;
    }
    s_auto_connecting = true;
    lock_give();

    ESP_LOGI(TAG, "Auto-connect: scanning for saved networks...");
    wifi_manager_scan_start(); /* 扫描同时让驱动缓存 AP 信息，替代旧版预扫描 */
}

/* ============ 发起连接（内部统一入口） ============ */

static esp_err_t start_connect(const char *ssid, const char *password, bool auto_mode) {
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;
    if (!s_wifi_initialized || !s_wifi_enabled) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Connecting to SSID: %s%s", ssid, auto_mode ? " (auto)" : "");

    lock_take();
    bool need_disconnect = (s_state == WIFI_MANAGER_CONNECTED ||
                            s_state == WIFI_MANAGER_CONNECTING);
    if (need_disconnect) {
        s_expect_disconnect = true; /* 主动切换网络，断开事件不算失败 */
        s_switch_pending = true;    /* 断开完成后由事件处理器续接连接 */
    }
    if (!auto_mode) {
        s_auto_connecting = false; /* 用户操作优先，终止自动连接流程 */
    }
    s_connect_attempts = 0;
    s_retry_count = 0;
    strncpy(s_connecting_ssid, ssid, sizeof(s_connecting_ssid) - 1);
    s_connecting_ssid[sizeof(s_connecting_ssid) - 1] = '\0';
    strncpy(s_connecting_pwd, password ? password : "", sizeof(s_connecting_pwd) - 1);
    s_connecting_pwd[sizeof(s_connecting_pwd) - 1] = '\0';
    s_fail_reason[0] = '\0';
    set_state_locked(WIFI_MANAGER_CONNECTING);
    lock_give();

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    /* 阈值取 OPEN 以兼容开放网络；实际安全性由 AP 协商决定 */
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    /* 关闭 PMF：WPA2/WPA3 混合路由下可避免首次连接因 PMF 协商失败报 AUTH_FAIL */
    wifi_config.sta.pmf_cfg.capable = false;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HASH_TO_ELEMENT;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        lock_take();
        s_switch_pending = false;
        set_state_locked(WIFI_MANAGER_IDLE);
        lock_give();
        return err;
    }

    connect_timeout_start();

    if (need_disconnect) {
        /* 正在连接/已连接时驱动会拒绝 connect：先断开，
         * DISCONNECTED 事件到达后由事件处理器续接 esp_wifi_connect() */
        esp_wifi_disconnect();
        return ESP_OK;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        /* STA 短暂未就绪等场景：通过 500ms 重试定时器再试（不阻塞调用者，
         * 本函数可能运行于 LVGL 线程，禁止 vTaskDelay） */
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s, will retry", esp_err_to_name(err));
        if (!s_retry_timer) {
            s_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(500),
                                         pdFALSE, NULL, wifi_retry_timer_cb);
        }
        if (s_retry_timer) {
            xTimerStop(s_retry_timer, 0);
            xTimerChangePeriod(s_retry_timer, pdMS_TO_TICKS(500), 0);
            xTimerStart(s_retry_timer, 0);
        }
    }
    return ESP_OK;
}

/* ============ WiFi 事件处理 ============ */

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (!s_wifi_enabled) return;
        /* 启动自动连接：扫描已保存网络 → 优先连接信号最强者 */
        auto_connect_begin();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t* done = (wifi_event_sta_scan_done_t*) event_data;

        if (done->status != 0 || done->number == 0) {
            ESP_LOGW(TAG, "Scan failed or empty (status=%d)", done->status);
            lock_take();
            s_scanning = false;
            if (s_auto_connecting) {
                /* 扫描失败：按保存顺序直接尝试 */
                s_auto_count = s_saved_count;
                s_auto_idx = 0;
                for (int i = 0; i < s_saved_count; i++) {
                    strncpy(s_auto_cands[i], s_saved[i].ssid, sizeof(s_auto_cands[0]) - 1);
                    s_auto_cands[i][sizeof(s_auto_cands[0]) - 1] = '\0';
                }
            }
            version_bump();
            bool go_auto = s_auto_connecting && s_auto_count > 0;
            lock_give();
            if (go_auto) auto_connect_next();
            return;
        }

        uint16_t got = done->number;
        if (got > WIFI_SCAN_MAX_AP) got = WIFI_SCAN_MAX_AP;
        if (esp_wifi_scan_get_ap_records(&got, s_scan_raw) != ESP_OK) {
            lock_take();
            s_scanning = false;
            version_bump();
            lock_give();
            return;
        }

        lock_take();
        /* 去重：相同 SSID 只保留 RSSI 最强的一条；驱动已按 RSSI 降序返回 */
        s_scan_count = 0;
        for (uint16_t i = 0; i < got && s_scan_count < WIFI_MANAGER_MAX_ITEMS; i++) {
            if (!s_scan_raw[i].ssid[0]) continue;
            bool dup = false;
            for (int j = 0; j < s_scan_count; j++) {
                if (strncmp(s_scan[j].ssid, (char*)s_scan_raw[i].ssid, 32) == 0) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
            strncpy(s_scan[s_scan_count].ssid, (char*)s_scan_raw[i].ssid, 32);
            s_scan[s_scan_count].ssid[32] = '\0';
            s_scan[s_scan_count].rssi = s_scan_raw[i].rssi;
            s_scan[s_scan_count].auth = s_scan_raw[i].authmode;
            s_scan_count++;
        }
        s_scanning = false;
        if (s_auto_connecting) {
            auto_connect_build_candidates();
        }
        version_bump();
        bool go_auto = s_auto_connecting && s_auto_count > 0;
        int unique = s_scan_count;
        lock_give();

        ESP_LOGI(TAG, "Scan done: %d APs (%d unique)", got, unique);
        if (go_auto) auto_connect_next();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t* e = (wifi_event_sta_connected_t*) event_data;
        lock_take();
        s_connecting_auth = e->authmode;
        lock_give();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* e = (wifi_event_sta_disconnected_t*) event_data;
        connect_timeout_stop();
        ESP_LOGW(TAG, "Disconnected from Wi-Fi, reason=%d", e->reason);

        bool was_connected = false;
        bool continue_connect = false;  /* 切网续接 / 重试 */
        uint32_t retry_delay = 0;
        bool next_auto = false;
        bool report_fail = false;
        char fail_txt[32] = "";

        lock_take();
        if (s_state == WIFI_MANAGER_DISABLED) {
            lock_give(); /* 停用 WiFi 引起，忽略 */
            return;
        }

        if (s_expect_disconnect) {
            s_expect_disconnect = false;
            s_current_ip[0] = '\0';
            s_connected_ssid[0] = '\0';
            if (s_state == WIFI_MANAGER_CONNECTED) {
                set_state_locked(WIFI_MANAGER_IDLE);
            } else if (s_state == WIFI_MANAGER_CONNECTING && s_fail_reason[0]) {
                /* 连接超时主动断开：上报失败 */
                set_state_locked(WIFI_MANAGER_FAILED);
                report_fail = true;
                strncpy(fail_txt, s_fail_reason, sizeof(fail_txt) - 1);
                fail_txt[sizeof(fail_txt) - 1] = '\0';
            } else if (s_state == WIFI_MANAGER_CONNECTING && s_switch_pending) {
                continue_connect = true; /* 切换网络：续接新目标连接 */
            }
            s_switch_pending = false;
            lock_give();
            if (report_fail) {
                ESP_LOGW(TAG, "WiFi connect failed: %s", fail_txt);
            }
            if (continue_connect) {
                esp_wifi_connect();
            }
            return;
        }

        was_connected = (s_state == WIFI_MANAGER_CONNECTED);
        s_current_ip[0] = '\0';
        s_connected_ssid[0] = '\0';
        s_rssi_valid = false;

        if (was_connected) {
            /* 已连接后掉线：记住目标网络，同网络指数退避重连（保持原有行为） */
            if (s_retry_count < WIFI_MAX_RETRY) {
                uint32_t delay_ms = WIFI_RETRY_BASE_MS * (1u << s_retry_count);
                if (delay_ms > 8000) delay_ms = 8000;
                s_retry_count++;
                retry_delay = delay_ms;
                set_state_locked(WIFI_MANAGER_CONNECTING);
            } else {
                ESP_LOGW(TAG, "WiFi reconnect failed after %d retries", WIFI_MAX_RETRY);
                strncpy(s_fail_reason, "连接断开", sizeof(s_fail_reason) - 1);
                s_fail_reason[sizeof(s_fail_reason) - 1] = '\0';
                set_state_locked(WIFI_MANAGER_FAILED);
                s_retry_count = 0;
                report_fail = true;
                strncpy(fail_txt, s_fail_reason, sizeof(fail_txt) - 1);
            }
        } else if (s_state == WIFI_MANAGER_CONNECTING) {
            /* 连接尝试失败：有界重试（WIFI_CONNECT_RETRY 次，1s/2s 递增延迟）。
             * 不对 AUTH_FAIL 特殊处理：本路由组合（WPA2/WPA3 混合 + 40MHz）
             * 冷启动后首次认证常瞬时失败 reason=202（路由器 PMKSA 残留/信道
             * 协商），重试即成功；真密码错误时认证快速失败，2 次重试约 3s
             * 后报错，仍符合"自动重试 1~2 次后显示失败"的约束 */
            if (s_connect_attempts < WIFI_CONNECT_RETRY) {
                s_connect_attempts++;
                retry_delay = WIFI_RETRY_BASE_MS * s_connect_attempts;
            } else if (s_auto_connecting) {
                next_auto = true; /* 换下一个已保存网络 */
            } else {
                strncpy(s_fail_reason, reason_to_text(e->reason), sizeof(s_fail_reason) - 1);
                s_fail_reason[sizeof(s_fail_reason) - 1] = '\0';
                set_state_locked(WIFI_MANAGER_FAILED);
                report_fail = true;
                strncpy(fail_txt, s_fail_reason, sizeof(fail_txt) - 1);
            }
        }
        lock_give();

        if (report_fail) {
            ESP_LOGW(TAG, "WiFi connect failed: %s", fail_txt);
        }
        if (next_auto) {
            auto_connect_next();
            return;
        }
        if (retry_delay > 0) {
            /* 立即重试走同一定时器路径，保证超时计时统一 */
            if (!s_retry_timer) {
                s_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(retry_delay),
                                             pdFALSE, NULL, wifi_retry_timer_cb);
            }
            if (s_retry_timer) {
                xTimerStop(s_retry_timer, 0);
                xTimerChangePeriod(s_retry_timer, pdMS_TO_TICKS(retry_delay), 0);
                xTimerStart(s_retry_timer, 0);
            } else {
                esp_wifi_connect();
            }
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        connect_timeout_stop();
        s_retry_count = 0;

        lock_take();
        snprintf(s_current_ip, sizeof(s_current_ip), IPSTR, IP2STR(&event->ip_info.ip));
        strncpy(s_connected_ssid,
                s_connecting_ssid[0] ? s_connecting_ssid : s_connected_ssid,
                sizeof(s_connected_ssid) - 1);
        s_connected_ssid[sizeof(s_connected_ssid) - 1] = '\0';
        s_auto_connecting = false;
        s_switch_pending = false;
        s_rssi_valid = false; /* 重新连接后重置 RSSI 缓存 */
        set_state_locked(WIFI_MANAGER_CONNECTED);
        /* 连接成功并获得有效 IP 后才保存凭据（避免保存不可用配置） */
        if (s_connected_ssid[0] && saved_find(s_connected_ssid) < 0) {
            saved_add(s_connected_ssid, s_connecting_pwd, s_connecting_auth);
        }
        lock_give();

        /* 联网成功，同步网络时间（保持原有行为） */
        sntp_sync_start();
    }
}

/* ============ 初始化 ============ */

void wifi_manager_init(void) {
    /* esp_wifi_init 只能调用一次，此函数幂等 */
    if (s_wifi_initialized) return;
    s_wifi_initialized = true;

    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    s_lock = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* 凭据由本模块自行管理（wifinet 命名空间），驱动配置不落盘 */
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* 加载已保存网络（含旧版 wifi_cfg 迁移），不在此处启动，
     * 由 wifi_manager_set_enabled() 根据设置控制并触发自动连接 */
    saved_load();
}

/**
 * 根据设置启停 Wi-Fi（STA）
 * @param enable true=启动并自动连接 / false=断开并停止
 */
void wifi_manager_set_enabled(bool enable) {
    if (enable) {
        if (!s_wifi_initialized) {
            wifi_manager_init();
        }
        if (!s_wifi_enabled) {
            /* 先置位再启动：STA_START 事件异步到达时回调才能通过使能检查 */
            s_wifi_enabled = true;
            esp_err_t err = esp_wifi_start();
            if (err != ESP_OK) {
                s_wifi_enabled = false;
                ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(err));
                return;
            }
            lock_take();
            if (s_state == WIFI_MANAGER_DISABLED) {
                set_state_locked(WIFI_MANAGER_IDLE); /* STA_START 事件接续自动连接 */
            }
            lock_give();
            ESP_LOGI(TAG, "Wi-Fi enabled");
        }
    } else {
        lock_take();
        bool was_enabled = s_wifi_enabled;
        s_auto_connecting = false;
        s_switch_pending = false;
        s_scanning = false;
        s_scan_count = 0;
        s_expect_disconnect = true;
        s_retry_count = 0;
        s_current_ip[0] = '\0';
        s_connected_ssid[0] = '\0';
        s_connecting_ssid[0] = '\0';
        s_fail_reason[0] = '\0';
        set_state_locked(WIFI_MANAGER_DISABLED);
        lock_give();

        if (was_enabled) {
            connect_timeout_stop();
            if (s_retry_timer) xTimerStop(s_retry_timer, 0);
            esp_wifi_disconnect();
            esp_wifi_stop();
            s_wifi_enabled = false;
            ESP_LOGI(TAG, "Wi-Fi disabled");
        }
    }
}

/* ============ 扫描 ============ */

esp_err_t wifi_manager_scan_start(void) {
    if (!s_wifi_initialized || !s_wifi_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    lock_take();
    if (s_scanning) {
        lock_give();
        return ESP_OK;
    }
    if (s_state == WIFI_MANAGER_CONNECTING && !s_auto_connecting) {
        /* 用户连接过程中不扫描，避免打断连接 */
        lock_give();
        return ESP_ERR_INVALID_STATE;
    }
    lock_give();

    /* 清理可能未取走的旧扫描结果，避免驱动缓冲残留导致报错 */
    esp_wifi_clear_ap_list();

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 0, .max = 120 } },
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        return err;
    }

    lock_take();
    s_scanning = true;
    version_bump();
    lock_give();
    ESP_LOGI(TAG, "Scan started");
    return ESP_OK;
}

/* 列表排序：已连接 > 已保存 > 其他；同组内 RSSI 降序 */
static int item_cmp(const void *a, const void *b) {
    const wifi_item_t *x = (const wifi_item_t *)a;
    const wifi_item_t *y = (const wifi_item_t *)b;
    if (x->connected != y->connected) return x->connected ? -1 : 1;
    if (x->saved != y->saved) return x->saved ? -1 : 1;
    return (int)y->rssi - (int)x->rssi;
}

size_t wifi_manager_get_items(wifi_item_t *items, size_t max_items) {
    if (!items || max_items == 0) return 0;

    wifi_item_t tmp[WIFI_MANAGER_MAX_ITEMS + WIFI_MANAGER_MAX_SAVED];
    size_t n = 0;

    lock_take();
    /* 1. 已保存网络（含扫描不可见的，rssi=-100 便于用户手动重试） */
    for (int i = 0; i < s_saved_count && n < sizeof(tmp) / sizeof(tmp[0]); i++) {
        memset(&tmp[n], 0, sizeof(tmp[n]));
        strncpy(tmp[n].ssid, s_saved[i].ssid, sizeof(tmp[n].ssid) - 1);
        tmp[n].auth_mode = (wifi_auth_mode_t)s_saved[i].auth_mode;
        tmp[n].saved = true;
        tmp[n].rssi = -100;
        /* 扫描可见则以扫描结果为准（RSSI/认证方式实时） */
        for (int j = 0; j < s_scan_count; j++) {
            if (strcmp(s_scan[j].ssid, s_saved[i].ssid) == 0) {
                tmp[n].rssi = s_scan[j].rssi;
                tmp[n].auth_mode = s_scan[j].auth;
                break;
            }
        }
        n++;
    }
    /* 2. 未保存的扫描结果 */
    for (int j = 0; j < s_scan_count && n < sizeof(tmp) / sizeof(tmp[0]); j++) {
        bool saved = false;
        for (int i = 0; i < s_saved_count; i++) {
            if (strcmp(s_saved[i].ssid, s_scan[j].ssid) == 0) { saved = true; break; }
        }
        if (saved) continue;
        memset(&tmp[n], 0, sizeof(tmp[n]));
        strncpy(tmp[n].ssid, s_scan[j].ssid, sizeof(tmp[n].ssid) - 1);
        tmp[n].rssi = s_scan[j].rssi;
        tmp[n].auth_mode = s_scan[j].auth;
        tmp[n].saved = false;
        n++;
    }
    /* 3. 标记当前连接/连接中 */
    for (size_t i = 0; i < n; i++) {
        if (s_connected_ssid[0] && strcmp(tmp[i].ssid, s_connected_ssid) == 0) {
            tmp[i].connected = true;
            tmp[i].saved = true;
            /* 已连接网络用实时 RSSI（节流缓存） */
            if (s_rssi_valid) tmp[i].rssi = s_cached_rssi;
        }
        if (s_state == WIFI_MANAGER_CONNECTING && s_connecting_ssid[0] &&
            strcmp(tmp[i].ssid, s_connecting_ssid) == 0) {
            tmp[i].connecting = true;
        }
    }
    lock_give();

    /* 4. 排序（连接 > 已保存 > 其他，组内 RSSI 降序） */
    qsort(tmp, n, sizeof(tmp[0]), item_cmp);

    if (n > max_items) n = max_items;
    memcpy(items, tmp, n * sizeof(items[0]));
    return n;
}

/* ============ 连接（公共接口） ============ */

esp_err_t wifi_manager_connect(const char *ssid, const char *password) {
    return start_connect(ssid, password, false);
}

esp_err_t wifi_manager_connect_saved(const char *ssid) {
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;

    lock_take();
    int idx = saved_find(ssid);
    if (idx < 0) {
        lock_give();
        return ESP_ERR_NOT_FOUND;
    }
    char pwd[WIFI_PWD_MAX];
    strncpy(pwd, s_saved[idx].password, sizeof(pwd) - 1);
    pwd[sizeof(pwd) - 1] = '\0';
    lock_give();

    return start_connect(ssid, pwd, false);
}

esp_err_t wifi_manager_disconnect(void) {
    if (!s_wifi_initialized || !s_wifi_enabled) return ESP_ERR_INVALID_STATE;

    lock_take();
    s_auto_connecting = false;
    s_switch_pending = false;
    s_expect_disconnect = true;
    s_current_ip[0] = '\0';
    s_connected_ssid[0] = '\0';
    set_state_locked(WIFI_MANAGER_IDLE);
    lock_give();

    connect_timeout_stop();
    if (s_retry_timer) xTimerStop(s_retry_timer, 0);
    esp_wifi_disconnect();
    return ESP_OK;
}

/* ============ 状态查询 ============ */

wifi_manager_state_t wifi_manager_get_state(void) {
    return s_state;
}

bool wifi_manager_is_scanning(void) {
    return s_scanning;
}

bool wifi_manager_is_enabled(void) {
    return s_wifi_enabled;
}

void wifi_manager_get_fail_reason(char *buf, size_t max_len) {
    if (!buf || max_len == 0) return;
    lock_take();
    strncpy(buf, s_fail_reason, max_len - 1);
    lock_give();
    buf[max_len - 1] = '\0';
}

bool wifi_manager_get_connected_ssid(char *buf, size_t max_len) {
    if (!buf || max_len == 0) return false;
    lock_take();
    bool ok = s_connected_ssid[0] != '\0';
    if (ok) {
        strncpy(buf, s_connected_ssid, max_len - 1);
        buf[max_len - 1] = '\0';
    }
    lock_give();
    return ok;
}

uint32_t wifi_manager_get_version(void) {
    return s_version;
}

/* ============ 兼容旧接口 ============ */

void wifi_manager_set_config(const char *ssid, const char *password) {
    if (!ssid || !password) return;

    ESP_LOGI(TAG, "Saving new Wi-Fi config. SSID: %s", ssid);

    lock_take();
    saved_add(ssid, password, WIFI_AUTH_WPA2_PSK);
    lock_give();

    /* WiFi 已开启则立即连接新配置；未开启则等下次启用时自动连接 */
    wifi_manager_connect(ssid, password);
}

bool wifi_manager_get_saved_config(char *ssid, size_t max_len) {
    if (!ssid || max_len == 0) return false;

    lock_take();
    bool ok = s_saved_count > 0;
    if (ok) {
        strncpy(ssid, s_saved[0].ssid, max_len - 1);
        ssid[max_len - 1] = '\0';
    }
    lock_give();
    return ok;
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

/* ============ 已保存网络查询/删除 ============ */

int wifi_manager_get_saved_count(void) {
    lock_take();
    int n = s_saved_count;
    lock_give();
    return n;
}

bool wifi_manager_get_saved_ssid(int idx, char *buf, size_t max_len) {
    if (!buf || max_len == 0) return false;
    lock_take();
    bool ok = idx >= 0 && idx < s_saved_count;
    if (ok) {
        strncpy(buf, s_saved[idx].ssid, max_len - 1);
        buf[max_len - 1] = '\0';
    }
    lock_give();
    return ok;
}

esp_err_t wifi_manager_remove_saved(const char *ssid) {
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;

    lock_take();
    bool is_current = s_connected_ssid[0] && strcmp(s_connected_ssid, ssid) == 0;
    saved_remove(ssid);
    lock_give();

    if (is_current) {
        wifi_manager_disconnect();
    }
    return ESP_OK;
}

/* ============ RSSI 查询（节流缓存） ============ */

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
        ESP_LOGD(TAG, "RSSI: %d dBm", ap_info.rssi);
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
