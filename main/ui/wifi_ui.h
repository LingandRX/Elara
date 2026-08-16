/**
 * @file wifi_ui.h
 * WiFi 管理页面：扫描列表 / 连接状态 / 密码输入
 *
 * 分层约定：本模块仅负责页面 UI，所有 WiFi 操作通过 wifi_manager API 执行，
 * 不直接调用 esp_wifi_* 底层接口；状态刷新通过轮询 wifi_manager_get_version()
 * 版本号实现（无线程安全问题，禁止在非 LVGL 上下文创建/修改控件）。
 */

#ifndef WIFI_UI_H
#define WIFI_UI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 WiFi 页面（创建后默认隐藏，需在 LVGL 上下文调用）
 */
void wifi_ui_init(void);

/**
 * 显示/隐藏 WiFi 页面
 * 显示时若无扫描结果会自动发起一次扫描
 */
void wifi_ui_show(bool show);

/**
 * 查询 WiFi 页面当前是否可见
 */
bool wifi_ui_is_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_UI_H */
