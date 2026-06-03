/**
 * @file xfer.h
 * 角色包传输协议（从 claude-desktop-buddy 的 xfer.h 迁移）
 */

#ifndef XFER_H
#define XFER_H

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化传输模块 */
void xfer_init(void);

/* 处理传输命令 */
bool xfer_command(cJSON *doc);

/* 查询传输状态 */
bool xfer_active(void);
uint32_t xfer_progress(void);
uint32_t xfer_total(void);

#ifdef __cplusplus
}
#endif

#endif /* XFER_H */
