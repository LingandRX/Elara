/**
 * @file storage_manager.c
 * LittleFS 存储管理实现 (替换 SPIFFS 以获得更快的格式化速度)
 */

#include "storage_manager.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <stdio.h>

static const char *TAG = "STORAGE";

esp_err_t storage_init(void) {
    ESP_LOGI(TAG, "Initializing LittleFS...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/spiffs", /* 保持路径不变，兼容现有代码 */
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    /* LittleFS 格式化极快，不需要特殊看门狗处理 */
    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format LittleFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS info (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LittleFS Mount Success. Total: %d bytes, Used: %d bytes", total, used);
    }

    return ESP_OK;
}

bool storage_file_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return false;
}

esp_err_t storage_mkdir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", path);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Created directory: %s", path);
    }
    return ESP_OK;
}

void storage_init_pet_dirs(void) {
    ESP_LOGI(TAG, "Initializing pet animation directories...");
    
    storage_mkdir("/spiffs/sprites");
    
    const char *dirs[] = {
        "/spiffs/sprites/idle",
        "/spiffs/sprites/run_right",
        "/spiffs/sprites/run_left",
        "/spiffs/sprites/waving",
        "/spiffs/sprites/jumping",
        "/spiffs/sprites/failed",
        "/spiffs/sprites/waiting",
        "/spiffs/sprites/action",
        "/spiffs/sprites/inspect",
        "/spiffs/sprites/deadloop"
    };
    
    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        storage_mkdir(dirs[i]);
    }
}

esp_err_t storage_get_info(size_t *total, size_t *used) {
    return esp_littlefs_info("storage", total, used);
}
