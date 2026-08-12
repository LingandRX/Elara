/**
 * @file sntp_sync.h
 * 网络时间同步 (SNTP)
 */

#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include <stdbool.h>

/**
 * 启动 SNTP 时间同步（幂等）
 * 联网成功（获取 IP）后调用
 */
void sntp_sync_start(void);

/**
 * 判断是否已通过网络同步到时间
 * @return true=已同步
 */
bool sntp_sync_is_synced(void);

#endif // SNTP_SYNC_H
