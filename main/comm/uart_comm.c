#include "uart_comm.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include "wifi_manager.h"

static const char *TAG = "UART";
static QueueHandle_t cmdQueue = NULL;

#define RX_BUF_SIZE     512

bool uart_comm_init(QueueHandle_t *queue) {
    cmdQueue = *queue;
    ESP_LOGI(TAG, "UART init OK (using stdin/stdout via USB-Serial/JTAG)");
    return true;
}

static void parse_terminal_cmd(const char *line) {
    // Check if it's a wifi configuration command: "wifi <ssid> <password>"
    if (strncmp(line, "wifi ", 5) == 0) {
        char ssid[64] = {0};
        char password[64] = {0};
        
        // Split command
        const char *ssid_start = line + 5;
        const char *space_pos = strchr(ssid_start, ' ');
        if (space_pos) {
            size_t ssid_len = space_pos - ssid_start;
            if (ssid_len < sizeof(ssid)) {
                strncpy(ssid, ssid_start, ssid_len);
            }
            const char *pass_start = space_pos + 1;
            strncpy(password, pass_start, sizeof(password) - 1);
            
            ESP_LOGI(TAG, "Received Wi-Fi config via terminal. SSID: %s", ssid);
            wifi_manager_set_config(ssid, password);
            return;
        }
        ESP_LOGW(TAG, "Invalid wifi command format. Usage: wifi <ssid> <password>");
        return;
    }

    cJSON *root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed: %s", line);
        uart_send_error("JSON parse error");
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        cJSON_Delete(root);
        return;
    }

    UartCmd cmd = {0};

    if (strcmp(type->valuestring, "status") == 0) {
        cJSON *state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state)) {
            cmd.type = CMD_STATUS;
            strncpy(cmd.state, state->valuestring, sizeof(cmd.state) - 1);
            xQueueSend(cmdQueue, &cmd, 0);
        }
    }
    else if (strcmp(type->valuestring, "chat") == 0) {
        cJSON *role = cJSON_GetObjectItem(root, "role");
        cJSON *text = cJSON_GetObjectItem(root, "text");
        cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
        cJSON *chunk = cJSON_GetObjectItem(root, "chunk");
        cJSON *seq = cJSON_GetObjectItem(root, "seq");

        if (cJSON_IsString(role) && cJSON_IsString(text)) {
            cmd.type = CMD_CHAT;
            strncpy(cmd.role, role->valuestring, sizeof(cmd.role) - 1);
            strncpy(cmd.text, text->valuestring, sizeof(cmd.text) - 1);
            if (cJSON_IsString(emotion)) {
                strncpy(cmd.emotion, emotion->valuestring, sizeof(cmd.emotion) - 1);
            }
            if (cJSON_IsBool(chunk)) {
                cmd.chunk = cJSON_IsTrue(chunk);
            }
            if (cJSON_IsNumber(seq)) {
                cmd.seq = seq->valueint;
            }
            xQueueSend(cmdQueue, &cmd, 0);
        }
    }
    else if (strcmp(type->valuestring, "cmd") == 0) {
        cJSON *action = cJSON_GetObjectItem(root, "action");
        if (cJSON_IsString(action) && strcmp(action->valuestring, "clear") == 0) {
            cmd.type = CMD_CLEAR;
            xQueueSend(cmdQueue, &cmd, 0);
        }
    }
    else if (strcmp(type->valuestring, "progress") == 0) {
        cJSON *value = cJSON_GetObjectItem(root, "value");
        if (cJSON_IsNumber(value)) {
            cmd.type = CMD_PROGRESS;
            cmd.progress = value->valueint;
            xQueueSend(cmdQueue, &cmd, 0);
        }
    }

    cJSON_Delete(root);
}

// UART 接收任务 - 通过 stdin (USB-Serial/JTAG) 读取数据
void uart_rx_task(void *pvParam) {
    uint8_t rxBuf[RX_BUF_SIZE];
    char lineBuf[512];
    int linePos = 0;

    ESP_LOGI(TAG, "UART RX task started, waiting for data on stdin...");

    int totalRx = 0;
    while (1) {
        int c = getchar();
        if (c != EOF) {
            totalRx++;
            if (c == '\n') {
                lineBuf[linePos] = '\0';
                if (linePos > 0) {
                    ESP_LOGI(TAG, "RX line (%d chars, total=%d): %s", linePos, totalRx, lineBuf);
                    parse_terminal_cmd(lineBuf);
                } else {
                    ESP_LOGW(TAG, "Empty line received");
                }
                linePos = 0;
            } else if (linePos < sizeof(lineBuf) - 1 && c >= 32) {
                lineBuf[linePos++] = (char)c;
            } else if (c == '\r') {
                // 忽略 \r
            } else {
                ESP_LOGW(TAG, "Ignored char: 0x%02X", (uint8_t)c);
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
