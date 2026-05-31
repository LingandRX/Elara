#include "tcp_server.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "uart_comm.h"

static const char *TAG = "TCP_SERVER";
#define PORT 8080

static void tcp_server_task(void *pvParameters) {
    char rx_buffer[1024];
    char addr_str[128];
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
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            }
            ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

            int len;
            do {
                len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (len < 0) {
                    ESP_LOGE(TAG, "Error occurred during recv: errno %d", errno);
                } else if (len == 0) {
                    ESP_LOGI(TAG, "Connection closed");
                } else {
                    rx_buffer[len] = 0;
                    char *line = strtok(rx_buffer, "\n");
                    while (line != NULL) {
                        char *cr = strchr(line, '\r');
                        if (cr) *cr = '\0';
                        if (strlen(line) > 0) {
                            comm_parse_cmd(line);
                        }
                        line = strtok(NULL, "\n");
                    }
                }
            } while (len > 0);

            shutdown(sock, 0);
            close(sock);
        }
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        close(listen_sock);
    }
    vTaskDelete(NULL);
}

void tcp_server_init(void) {
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}
