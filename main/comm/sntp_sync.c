/**
 * @file sntp_sync.c
 * 网络时间同步 (SNTP)
 * 通过 NTP 服务器获取网络时间并写入系统 RTC
 */

#include "sntp_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <stdlib.h>
#include <time.h>

static const char *TAG = "SNTP_SYNC";
static bool s_sntp_started = false;

void sntp_sync_start(void) {
    if (s_sntp_started) return;
    s_sntp_started = true;

    /* 本地时区：中国标准时间 (UTC+8)
     * POSIX TZ 中符号相反：CST-8 表示 UTC+8 */
    setenv("TZ", "CST-8", 1);
    tzset();

    ESP_LOGI(TAG, "Starting SNTP time sync...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized");
}

bool sntp_sync_is_synced(void) {
    /* 不依赖 SNTP 状态（COMPLETED 同步完成后会重置），
     * 直接检测系统时间是否已被设置为有效 epoch */
    return s_sntp_started && (time(NULL) > 1600000000);
}
