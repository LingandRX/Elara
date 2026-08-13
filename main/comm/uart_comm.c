#include "uart_comm.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "wifi_manager.h"
#include "tcp_server.h"
#include "ui/pet_ui.h"
#include "display/lv_port_disp.h"
#include "buddy/buddy_state.h"
#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag.h"
#endif

static const char *TAG = "COMM";
static QueueHandle_t cmdQueue = NULL;

/* 上传状态管理 */
static bool s_upload_mode = false;
static uint32_t s_upload_size = 0;
static uint32_t s_uploaded_bytes = 0;
static FILE *s_upload_file = NULL;
static comm_upload_done_cb_t s_upload_done_cb = NULL;

#define RX_BUF_SIZE     512

/* 非阻塞控制台输出: USB 未连接主机时 printf 会因 TX 缓冲满而永久阻塞,
 * WiFi 场景下不能依赖控制台输出. 缓冲满时丢弃而非阻塞.
 * 注意: 本固件控制台走 ROM CDC (esp_usb_cdc_rom_console), usb_serial_jtag
 * 驱动未安装, 首次探测失败后静默跳过, 避免每次调用刷错误日志. */
static bool s_comm_console_ready = true;

static void comm_console_write_len(const char *s, size_t len) {
    /* 设备→主机命令 (审批结果/事件/xack) 同时回传当前 TCP 客户端
     * (WiFi/TCP 场景下 USB 串口不可用, 否则审批回复会丢失) */
    tcp_server_send(s, len);
#ifdef CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    if (!s_comm_console_ready) return;
    size_t sent = 0;
    while (sent < len) {
        size_t n = usb_serial_jtag_write_bytes(s + sent, len - sent, 0);
        if (n == 0) {
            s_comm_console_ready = false;  /* 驱动未安装或缓冲满: 放弃输出 */
            return;
        }
        sent += n;
    }
#else
    printf("%.*s", (int)len, s);
    fflush(stdout);
#endif
}

static void comm_console_write(const char *s) {
    comm_console_write_len(s, strlen(s));
}

void comm_set_upload_done_cb(comm_upload_done_cb_t cb) {
    s_upload_done_cb = cb;
}

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

    /* 文件已落盘, 先通知 TCP 立即回传 finished (避免被后续刷新/打印阻塞拖住) */
    if (s_upload_done_cb) s_upload_done_cb();

    /* 刷新 UI 缓存 (加超时, 拿不到锁则跳过刷新, 不能阻塞上传流程) */
    if (lv_port_disp_lock(500)) {
        pet_ui_refresh();
        pet_ui_show(true);  /* 上传完成后直接展示新精灵图 */
        lv_port_disp_unlock();
    }

    uart_send_event("upload", "finished");
}

bool comm_start_upload(const char *path, uint32_t size) {
    s_upload_size = size;
    s_uploaded_bytes = 0;

    if (s_upload_file) {
        fclose(s_upload_file);
        s_upload_file = NULL;
    }

    char target_path[128];
    if (path && strlen(path) > 0) {
        if (path[0] == '/') {
            /* 绝对路径：直接使用 */
            strncpy(target_path, path, sizeof(target_path) - 1);
        } else {
            /* 相对路径：自动补全宠物动画根目录 */
            snprintf(target_path, sizeof(target_path), "/spiffs/sprites/%s", path);
        }
    } else {
        /* 默认路径：精灵图 */
        strncpy(target_path, "/spiffs/sprite.png", sizeof(target_path) - 1);
    }

    s_upload_file = fopen(target_path, "wb");
    if (s_upload_file) {
        s_upload_mode = true;
        ESP_LOGI(TAG, "Binary upload started to %s: %d bytes", target_path, size);
        uart_send_event("upload", "ready");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to open %s: %s", target_path, strerror(errno));
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
    
    /* 计算并打印进度日志 */
    uint32_t old_progress = (s_uploaded_bytes * 100) / s_upload_size;
    s_uploaded_bytes += written;
    uint32_t new_progress = (s_uploaded_bytes * 100) / s_upload_size;

    /* 每增长 10% 打印一次日志，避免日志过多冲击串口 */
    if ((new_progress / 10 > old_progress / 10) || new_progress == 100) {
        ESP_LOGI(TAG, "Upload progress: %d%% (%d/%d bytes)", (int)new_progress, s_uploaded_bytes, s_upload_size);
        
        /* 向上位机发送进度事件 (非阻塞, 防止控制台无主机读取时阻塞上传) */
        char msg[96];
        int n = snprintf(msg, sizeof(msg), "{\"type\":\"event\",\"source\":\"upload\",\"action\":\"progress\",\"value\":%d}\n", (int)new_progress);
        if (n > 0) comm_console_write(msg);
    }

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
    ESP_LOGI(TAG, "CMD RX: %s", line);
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
            cJSON *path = cJSON_GetObjectItem(root, "path");
            if (cJSON_IsNumber(size)) {
                comm_start_upload(cJSON_IsString(path) ? path->valuestring : NULL, size->valueint);
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
    /* 同时传递给 buddy_state 处理（权限、时间同步等）。
     * 注意: linebuf_feed 以 '\n' 作为行结束触发 apply_json, 而这里收到的
     * 是已去掉换行的完整行, 必须补一个 '\n' 才能让状态机真正解析,
     * 否则 session/prompt/time 等数据只会堆积在行缓冲里, connected 恒为 false。 */
    buddy_feed_usb_line(line, strlen(line));
    buddy_feed_usb_line("\n", 1);
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

void uart_send_raw(const char *data, size_t len) {
    comm_console_write_len(data, len);
}

void uart_send_event(const char *source, const char *action) {
    char msg[96];
    int n = snprintf(msg, sizeof(msg), "{\"type\":\"event\",\"source\":\"%s\",\"action\":\"%s\"}\n", source, action);
    if (n > 0) comm_console_write(msg);
}

void uart_send_error(const char *msg) {
    char buf[96];
    int n = snprintf(buf, sizeof(buf), "{\"type\":\"error\",\"msg\":\"%s\"}\n", msg);
    if (n > 0) comm_console_write(buf);
}
