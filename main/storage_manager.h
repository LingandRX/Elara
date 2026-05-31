/**
 * @file storage_manager.h
 * SPIFFS 存储管理
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 SPIFFS
 * @return ESP_OK if success
 */
esp_err_t storage_init(void);

/**
 * 获取文件是否存在
 * @param path 文件路径
 * @return true if exists
 */
bool storage_file_exists(const char *path);

/**
 * 获取可用空间
 * @param total 总空间 (字节)
 * @param used 已用空间 (字节)
 * @return ESP_OK if success
 */
esp_err_t storage_get_info(size_t *total, size_t *used);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_MANAGER_H */
