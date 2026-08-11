#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// 命令类型
typedef enum {
    CMD_STATUS,     // {"type":"status","state":"..."}
    CMD_CHAT,       // {"type":"chat","role":"...","text":"..."}
    CMD_CLEAR,      // {"type":"cmd","action":"clear"}
    CMD_PROGRESS,   // {"type":"progress","value":0-100}
    CMD_PETDEX,     // {"type":"petdex","state":"..."}
    CMD_UPLOAD,     // {"type":"upload","size":123456}
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
    uint32_t size;      // for CMD_UPLOAD
} UartCmd;

// 初始化 UART0 (GPIO43/44, 115200)
bool uart_comm_init(QueueHandle_t *cmdQueue);

// 上传控制接口 (供 UART 和 TCP 共享)
bool comm_start_upload(const char *path, uint32_t size);
void comm_write_upload_byte(uint8_t c);
bool comm_is_uploading(void);
size_t comm_write_upload_data(const uint8_t *data, size_t len);

// 发送原始数据到上位机
void uart_send_raw(const char *data, size_t len);

// 发送事件到上位机
void uart_send_event(const char *source, const char *action);
void uart_send_error(const char *msg);

// UART 接收任务（内部创建）
void uart_rx_task(void *pvParam);

// 解析通用命令（暴露给 TCP 等其他模块使用）
void comm_parse_cmd(const char *line);

// 上传完成回调（文件已落盘时触发，用于 TCP 立即回传 finished，
// 避免被控制台 printf 阻塞拖住）
typedef void (*comm_upload_done_cb_t)(void);
void comm_set_upload_done_cb(comm_upload_done_cb_t cb);

#endif
