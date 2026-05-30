#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 命令类型
typedef enum {
    CMD_STATUS,     // {"type":"status","state":"..."}
    CMD_CHAT,       // {"type":"chat","role":"...","text":"..."}
    CMD_CLEAR,      // {"type":"cmd","action":"clear"}
    CMD_PROGRESS,   // {"type":"progress","value":0-100}
    CMD_UNKNOWN
} CmdType;

typedef struct {
    CmdType type;
    char state[16];     // for CMD_STATUS
    char role[8];       // for CMD_CHAT
    char text[256];     // for CMD_CHAT
    char emotion[16];   // for CMD_CHAT
    bool chunk;         // for CMD_CHAT
    int seq;            // for CMD_CHAT
    int progress;       // for CMD_PROGRESS (0-100)
} UartCmd;

// 初始化 UART0 (GPIO43/44, 115200)
bool uart_comm_init(QueueHandle_t *cmdQueue);

// 发送事件到上位机
void uart_send_event(const char *source, const char *action);
void uart_send_error(const char *msg);

// UART 接收任务（内部创建）
void uart_rx_task(void *pvParam);

#endif
