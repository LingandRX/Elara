#include "uart_comm.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "wifi_manager.h"

static const char *TAG = "COMM";
static QueueHandle_t cmdQueue = NULL;

/* 上传状态管理 */
static bool s_upload_mode = false;
static uint32_t s_upload_size = 0;
static uint32_t s_uploaded_bytes = 0;
static FILE *s_upload_file = NULL;

#define RX_BUF_SIZE     512

bool uart_comm_init(QueueHandle_t *queue) {
    cmdQueue = *queue;
    ESP_LOGI(TAG, "COMM init OK");
    return true;
}

bool comm_is_uploading(void) {
    return s_upload_mode;
}

static void comm_finish_upload(void) {
    if (s_upload_file) {
        fclose(s_upload_file);
        s_upload_file = NULL;
    }
    s_upload_mode = false;
    ESP_LOGI(TAG, "Upload finished: %d bytes", s_uploaded_bytes);
    uart_send_event("upload", "finished");
}

bool comm_start_upload(uint32_t size) {
    s_upload_size = size;
    s_uploaded_bytes = 0;

    if (s_upload_file) {
        fclose(s_upload_file);
        s_upload_file = NULL;
    }

    /* 覆盖旧文件 */
    s_upload_file = fopen("/spiffs/sprite.png", "wb");
    if (s_upload_file) {
        s_upload_mode = true;
        ESP_LOGI(TAG, "Binary upload started: %d bytes", size);
        uart_send_event("upload", "ready");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to open /spiffs/sprite.png: %s", strerror(errno));
        uart_send_error("file open error");
        return false;
    }
}

void comm_write_upload_byte(uint8_t c) {
    comm_write_upload_data(&c, 1);
}

size_t comm_write_upload_data(const uint8_t *data, size_t len) {
    if (!s_upload_mode || !s_upload_file || !data || len == 0) return 0;

    size_t remaining = s_upload_size - s_uploaded_bytes;
    size_t to_write = len > remaining ? remaining : len;
    size_t written = fwrite(data, 1, to_write, s_upload_file);
    s_uploaded_bytes += written;

    if (written != to_write) {
        ESP_LOGE(TAG, "Upload write failed: %s", strerror(errno));
        fclose(s_upload_file);
        s_upload_file = NULL;
        s_upload_mode = false;
        uart_send_error("file write error");
        return written;
    }

    if (s_uploaded_bytes >= s_upload_size) comm_finish_upload();
    return written;
}

void comm_parse_cmd(const char *line) {
    if (strncmp(line, "wifi ", 5) == 0) {
        char ssid[64] = {0}, password[64] = {0};
        const char *ssid_start = line + 5;
        const char *space_pos = strchr(ssid_start, ' ');
        if (space_pos) {
            strncpy(ssid, ssid_start, space_pos - ssid_start);
            strncpy(password, space_pos + 1, sizeof(password) - 1);
            wifi_manager_set_config(ssid, password);
            return;
        }
    }

    cJSON *root = cJSON_Parse(line);
    if (!root) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (cJSON_IsString(type)) {
        UartCmd cmd = {0};
        if (strcmp(type->valuestring, "status") == 0) {
            cJSON *state = cJSON_GetObjectItem(root, "state");
            if (cJSON_IsString(state)) {
                cmd.type = CMD_STATUS;
                strncpy(cmd.state, state->valuestring, sizeof(cmd.state) - 1);
                xQueueSend(cmdQueue, &cmd, 0);
            }
        } else if (strcmp(type->valuestring, "upload") == 0) {
            cJSON *size = cJSON_GetObjectItem(root, "size");
            if (cJSON_IsNumber(size)) {
                comm_start_upload(size->valueint);
            }
        } else if (strcmp(type->valuestring, "petdex") == 0) {
            cJSON *state = cJSON_GetObjectItem(root, "state");
            if (cJSON_IsString(state)) {
                cmd.type = CMD_PETDEX;
                strncpy(cmd.state, state->valuestring, sizeof(cmd.state) - 1);
                xQueueSend(cmdQueue, &cmd, 0);
            }
        } else if (strcmp(type->valuestring, "chat") == 0) {
            cJSON *role = cJSON_GetObjectItem(root, "role");
            cJSON *text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(role) && cJSON_IsString(text)) {
                cmd.type = CMD_CHAT;
                strncpy(cmd.role, role->valuestring, sizeof(cmd.role) - 1);
                strncpy(cmd.text, text->valuestring, sizeof(cmd.text) - 1);
                xQueueSend(cmdQueue, &cmd, 0);
            }
        }
    }
    cJSON_Delete(root);
}

void uart_rx_task(void *pvParam) {
    char lineBuf[512];
    int linePos = 0;
    while (1) {
        int c = getchar();
        if (c != EOF) {
            if (s_upload_mode) {
                comm_write_upload_byte((uint8_t)c);
                continue;
            }
            if (c == '\n') {
                lineBuf[linePos] = '\0';
                if (linePos > 0) comm_parse_cmd(lineBuf);
                linePos = 0;
            } else if (linePos < sizeof(lineBuf) - 1 && c >= 32) {
                lineBuf[linePos++] = (char)c;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void uart_send_event(const char *source, const char *action) {
    printf("{\"type\":\"event\",\"source\":\"%s\",\"action\":\"%s\"}\n", source, action);
    fflush(stdout);
}

void uart_send_error(const char *msg) {
    printf("{\"type\":\"error\",\"msg\":\"%s\"}\n", msg);
    fflush(stdout);
}
