#include "tcp_server.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "uart_comm.h"
#include "cJSON.h"

static const char *TAG = "TCP_SERVER";
#define PORT 8080
#define TCP_SERVER_STACK_SIZE 8192

static uint8_t s_rx_buffer[1024];
static char s_line_buffer[1025];
static char s_addr_str[128];

/* 当前进行 TCP 上传的连接 socket (用于 upload 完成回调回传 finished) */
static int s_upload_sock = -1;

static int send_all(int sock, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int ret = send(sock, data + sent, len - sent, 0);
        if (ret < 0) return ret;
        sent += ret;
    }
    return 0;
}

static void tcp_send_event(int sock, const char *source, const char *action) {
    char msg[96];
    int len = snprintf(msg, sizeof(msg),
                       "{\"type\":\"event\",\"source\":\"%s\",\"action\":\"%s\"}\n",
                       source, action);
    if (len > 0) send_all(sock, msg, (size_t)len);
}

static void tcp_send_error(int sock, const char *msg_text) {
    char msg[96];
    int len = snprintf(msg, sizeof(msg), "{\"type\":\"error\",\"msg\":\"%s\"}\n", msg_text);
    if (len > 0) send_all(sock, msg, (size_t)len);
}

/* 上传完成回调: 文件落盘后由 uart_comm 触发, 立即回传 finished */
static void tcp_upload_done_handler(void) {
    if (s_upload_sock >= 0) {
        tcp_send_event(s_upload_sock, "upload", "finished");
    }
}

static bool is_upload_command(const char *line) {
    bool result = false;
    cJSON *root = cJSON_Parse(line);
    if (root) {
        cJSON *type = cJSON_GetObjectItem(root, "type");
        result = cJSON_IsString(type) && strcmp(type->valuestring, "upload") == 0;
        cJSON_Delete(root);
    }
    return result;
}

static void tcp_server_task(void *pvParameters) {
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);

        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
            close(listen_sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "TCP Server listening on port %d", PORT);

        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t addr_len = sizeof(source_addr);
            int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
                break;
            }

            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, s_addr_str, sizeof(s_addr_str) - 1);
            }
            ESP_LOGI(TAG, "Socket accepted ip address: %s", s_addr_str);

            /* 关闭 Nagle, 让 ready/finished 等事件立即发出 */
            int nodelay = 1;
            setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

            bool tcp_upload = false;
            int len;
            do {
                len = recv(sock, s_rx_buffer, sizeof(s_rx_buffer), 0);
                if (len < 0) {
                    ESP_LOGE(TAG, "Error occurred during recv: errno %d", errno);
                } else if (len == 0) {
                    ESP_LOGI(TAG, "Connection closed");
                } else {
                    /* TCP 上传握手完成后，后续数据按原始二进制写入文件 */
                    if (tcp_upload && comm_is_uploading()) {
                        size_t written = comm_write_upload_data(s_rx_buffer, (size_t)len);
                        if (written < (size_t)len && comm_is_uploading()) {
                            tcp_send_error(sock, "file write error");
                            break;
                        }
                        /* finished 由 comm_finish_upload 内的回调发送 */
                    } else {
                        /* 解析 JSON 命令 */
                        memcpy(s_line_buffer, s_rx_buffer, (size_t)len);
                        s_line_buffer[len] = '\0';

                        /* 首个换行之后的字节可能是 upload 二进制数据 (防 TCP 粘包丢数据) */
                        char *first_nl = strchr(s_line_buffer, '\n');
                        size_t bin_off = first_nl ? (size_t)(first_nl - s_line_buffer) + 1 : (size_t)len;
                        bool upload_triggered = false;

                        char *line = strtok(s_line_buffer, "\n");
                        while (line != NULL) {
                            char *cr = strchr(line, '\r');
                            if (cr) *cr = '\0';
                            if (strlen(line) > 0) {
                                bool upload_cmd = is_upload_command(line);
                                comm_parse_cmd(line);
                                if (upload_cmd) {
                                    if (comm_is_uploading()) {
                                        tcp_upload = true;
                                        upload_triggered = true;
                                        s_upload_sock = sock;
                                        tcp_send_event(sock, "upload", "ready");
                                    } else {
                                        tcp_send_error(sock, "upload start failed");
                                    }
                                    /* 上传开始后, 剩余字节全部为二进制, 停止行解析 */
                                    break;
                                }
                            }
                            line = strtok(NULL, "\n");
                        }

                        /* 换行之后的字节写回上传文件 (避免与 JSON 同包到达时丢失) */
                        if (upload_triggered && tcp_upload && comm_is_uploading() && bin_off < (size_t)len) {
                            size_t remain = (size_t)len - bin_off;
                            size_t written = comm_write_upload_data((const uint8_t *)(s_line_buffer + bin_off), remain);
                            if (written < remain && comm_is_uploading()) {
                                tcp_send_error(sock, "file write error");
                            }
                        }
                    }
                }
            } while (len > 0);

            if (s_upload_sock == sock) s_upload_sock = -1;
            shutdown(sock, 0);
            close(sock);
        }
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        close(listen_sock);
    }
    vTaskDelete(NULL);
}

void tcp_server_init(void) {
    comm_set_upload_done_cb(tcp_upload_done_handler);
    xTaskCreate(tcp_server_task, "tcp_server", TCP_SERVER_STACK_SIZE, NULL, 5, NULL);
}
