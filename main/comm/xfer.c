/**
 * @file xfer.c
 * 角色包传输协议实现
 */

#include "xfer.h"
#include "buddy/buddy_state.h"
#include "buddy/buddy_stats.h"
#include "comm/ble_bridge.h"
#include "storage_manager.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "XFER";

/* 传输状态 */
static FILE    *s_xfile = NULL;
static uint32_t s_x_expected = 0;
static uint32_t s_x_written = 0;
static char     s_x_char_name[24] = "";
static bool     s_x_active = false;
static uint32_t s_x_total = 0;
static uint32_t s_x_total_written = 0;

/* 外部声明 */
extern void uart_send_raw(const char *data, size_t len);

static void xack(const char *what, bool ok, uint32_t n) {
    char b[128];
    int len = snprintf(b, sizeof(b),
                       "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu}\n",
                       what, ok ? "true" : "false", (unsigned long)n);
    uart_send_raw(b, len);
    ble_write((const uint8_t *)b, len);
}

__attribute__((unused))
static uint32_t xwipe_dir(const char *dir) {
    /* 简化：仅删除文件，不递归删除子目录 */
    ESP_LOGW(TAG, "x wipe dir not fully implemented");
    return 0;
}

__attribute__((unused))
static uint32_t xwipe_all_chars(void) {
    /* 简化实现 */
    ESP_LOGI(TAG, "Wiping all characters");
    return 0;
}

void xfer_init(void) {
    s_x_active = false;
    s_xfile = NULL;
}

bool xfer_command(cJSON *doc) {
    cJSON *cmd_obj = cJSON_GetObjectItem(doc, "cmd");
    if (!cJSON_IsString(cmd_obj)) return false;
    const char *cmd = cmd_obj->valuestring;

    if (strcmp(cmd, "name") == 0) {
        cJSON *name = cJSON_GetObjectItem(doc, "name");
        if (cJSON_IsString(name)) pet_name_set(name->valuestring);
        xack("name", cJSON_IsString(name), 0);
        return true;
    }

    if (strcmp(cmd, "species") == 0) {
        uint8_t idx = 0xFF;
        cJSON *idx_obj = cJSON_GetObjectItem(doc, "idx");
        if (cJSON_IsNumber(idx_obj)) idx = (uint8_t)idx_obj->valuedouble;
        species_idx_save(idx);
        /* buddy_mode 和 buddySetSpeciesIdx 由调用者处理 */
        xack("species", true, 0);
        return true;
    }

    if (strcmp(cmd, "unpair") == 0) {
        ble_clear_bonds();
        xack("unpair", true, 0);
        return true;
    }

    if (strcmp(cmd, "owner") == 0) {
        cJSON *name = cJSON_GetObjectItem(doc, "name");
        if (cJSON_IsString(name)) owner_name_set(name->valuestring);
        xack("owner", cJSON_IsString(name), 0);
        return true;
    }

    if (strcmp(cmd, "status") == 0) {
        /* 返回状态快照 */
        char b[512];
        BuddyStats *st = buddy_stats_get();
        int len = snprintf(b, sizeof(b),
            "{\"ack\":\"status\",\"ok\":true,\"n\":0,\"data\":{"
            "\"name\":\"%s\",\"owner\":\"%s\",\"sec\":%s,"
            "\"stats\":{\"appr\":%u,\"deny\":%u,\"vel\":%u,\"nap\":%lu,\"lvl\":%u}"
            "}}\n",
            pet_name_get(), owner_name_get(),
            ble_secure() ? "true" : "false",
            st->approvals, st->denials, buddy_stats_median_velocity(),
            (unsigned long)st->nap_seconds, st->level
        );
        uart_send_raw(b, len);
        ble_write((const uint8_t *)b, len);
        return true;
    }

    if (strcmp(cmd, "char_begin") == 0) {
        cJSON *name = cJSON_GetObjectItem(doc, "name");
        cJSON *total = cJSON_GetObjectItem(doc, "total");
        if (cJSON_IsString(name)) {
            strncpy(s_x_char_name, name->valuestring, sizeof(s_x_char_name) - 1);
            s_x_char_name[sizeof(s_x_char_name) - 1] = '\0';
        } else {
            strcpy(s_x_char_name, "pet");
        }
        s_x_total = cJSON_IsNumber(total) ? (uint32_t)total->valuedouble : 0;
        s_x_total_written = 0;
        s_x_active = true;
        xack("char_begin", true, 0);
        return true;
    }

    if (!s_x_active) {
        /* permission 命令不是传输命令 */
        return strcmp(cmd, "permission") != 0;
    }

    if (strcmp(cmd, "file") == 0) {
        cJSON *path = cJSON_GetObjectItem(doc, "path");
        cJSON *size = cJSON_GetObjectItem(doc, "size");
        s_x_expected = cJSON_IsNumber(size) ? (uint32_t)size->valuedouble : 0;
        s_x_written = 0;
        if (cJSON_IsString(path)) {
            char full[128];
            snprintf(full, sizeof(full), "/spiffs/characters/%s/%s", s_x_char_name, path->valuestring);
            storage_mkdir("/spiffs/characters");
            char dir[64];
            snprintf(dir, sizeof(dir), "/spiffs/characters/%s", s_x_char_name);
            storage_mkdir(dir);
            s_xfile = fopen(full, "wb");
        }
        xack("file", s_xfile != NULL, 0);
        return true;
    }

    if (strcmp(cmd, "chunk") == 0) {
        cJSON *b64 = cJSON_GetObjectItem(doc, "d");
        if (!cJSON_IsString(b64) || !s_xfile) {
            xack("chunk", false, 0);
            return true;
        }
        const char *b64str = b64->valuestring;
        uint8_t buf[400];
        size_t out_len = 0;
        int rc = mbedtls_base64_decode(buf, sizeof(buf), &out_len,
                                       (const uint8_t *)b64str, strlen(b64str));
        if (rc != 0) {
            xack("chunk", false, 0);
            return true;
        }
        fwrite(buf, 1, out_len, s_xfile);
        s_x_written += out_len;
        s_x_total_written += out_len;
        xack("chunk", true, s_x_written);
        return true;
    }

    if (strcmp(cmd, "file_end") == 0) {
        bool ok = s_xfile && (s_x_written == s_x_expected || s_x_expected == 0);
        if (s_xfile) {
            fclose(s_xfile);
            s_xfile = NULL;
        }
        xack("file_end", ok, s_x_written);
        return true;
    }

    if (strcmp(cmd, "char_end") == 0) {
        s_x_active = false;
        /* 简化：不自动加载 GIF，仅标记可用 */
        xack("char_end", true, 0);
        return true;
    }

    return false;
}

bool xfer_active(void) { return s_x_active; }
uint32_t xfer_progress(void) { return s_x_total_written; }
uint32_t xfer_total(void) { return s_x_total; }
