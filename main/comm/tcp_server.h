#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stddef.h>

void tcp_server_init(void);

/* 通过当前 TCP 客户端连接回传数据 (设备→主机), 无连接时返回 0 */
int tcp_server_send(const char *data, size_t len);

#endif // TCP_SERVER_H
