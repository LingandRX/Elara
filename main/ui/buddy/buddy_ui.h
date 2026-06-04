/**
 * @file buddy_ui.h
 * Buddy 主页面 UI
 * 从 claude-desktop-buddy 的 main.cpp 迁移
 */

#ifndef BUDDY_UI_H
#define BUDDY_UI_H

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 显示模式 ============ */
typedef enum {
    BUDDY_MODE_NORMAL = 0,
    BUDDY_MODE_PET,
    BUDDY_MODE_INFO,
    BUDDY_MODE_MAX
} BuddyMode;

/* ============ Info 页面索引 ============ */
typedef enum {
    INFO_PAGE_ABOUT = 0,
    INFO_PAGE_BUTTONS,
    INFO_PAGE_CLAUDE,
    INFO_PAGE_DEVICE,
    INFO_PAGE_BT,
    INFO_PAGE_BATTERY,
    INFO_PAGE_CREDITS,
    INFO_PAGE_MAX
} InfoPageIdx;

/* ============ 菜单项 ============ */
typedef enum {
    BUDDY_MENU_SETTINGS = 0,
    BUDDY_MENU_SHUTDOWN,
    BUDDY_MENU_HELP,
    BUDDY_MENU_ABOUT,
    BUDDY_MENU_DEMO,
    BUDDY_MENU_CLOSE,
    BUDDY_MENU_MAX
} BuddyMenuItem;

/* ============ 设置项 ============ */
typedef enum {
    BUDDY_SET_BRIGHTNESS = 0,
    BUDDY_SET_SOUND,
    BUDDY_SET_BT,
    BUDDY_SET_WIFI,
    BUDDY_SET_LED,
    BUDDY_SET_HUD,
    BUDDY_SET_ROTATE,
    BUDDY_SET_ASCII,
    BUDDY_SET_AUTO_SLEEP,
    BUDDY_SET_RESET,
    BUDDY_SET_BACK,
    BUDDY_SET_MAX
} BuddySettingItem;

/* ============ 外部状态同步回调 ============ */
typedef void (*BuddyOverlayCloseCb)(void);

/**
 * 注册覆盖层点击外部时的关闭回调
 * 用于同步 main.c 中的 BuddyUIState 状态
 */
void buddy_ui_set_overlay_close_cb(BuddyOverlayCloseCb menu_cb,
                                    BuddyOverlayCloseCb settings_cb,
                                    BuddyOverlayCloseCb approval_cb);

/* ============ 初始化与显示 ============ */

/**
 * 初始化 Buddy UI
 */
void buddy_ui_init(void);

/**
 * 显示/隐藏 Buddy UI
 * @param show 是否显示
 */
void buddy_ui_show(bool show);

/**
 * 判断 Buddy UI 是否可见
 * @return true=可见
 */
bool buddy_ui_is_visible(void);

/**
 * 获取 UI 主容器
 * @return lv_obj_t 指针
 */
lv_obj_t *buddy_ui_get_container(void);

/* ============ 模式切换 ============ */

/**
 * 设置显示模式
 * @param mode 模式
 */
void buddy_ui_set_mode(BuddyMode mode);

/**
 * 获取当前显示模式
 * @return 当前模式
 */
BuddyMode buddy_ui_get_mode(void);

/* ============ 主页 (NORMAL) ============ */

/**
 * 设置 HUD 文本
 * @param text 文本内容
 */
void buddy_ui_set_hud_text(const char *text);

/**
 * 显示/隐藏 HUD
 * @param visible 是否可见
 */
void buddy_ui_set_hud_visible(bool visible);

/**
 * 显示审批弹窗
 * @param tool_name 工具名
 * @param prompt 提示文本
 */
void buddy_ui_show_approval(const char *tool_name, const char *prompt);

/**
 * 隐藏审批弹窗
 */
void buddy_ui_hide_approval(void);

/**
 * 判断审批弹窗是否显示
 * @return true=显示中
 */
bool buddy_ui_is_approval_visible(void);

/* ============ PET 页面 ============ */

/**
 * 设置宠物统计值
 * @param mood   心情 (0-100)
 * @param fed    饱食度 (0-100)
 * @param energy 能量 (0-100)
 * @param level  等级 (>=1)
 */
void buddy_ui_set_pet_stats(int mood, int fed, int energy, int level);

/* ============ INFO 页面 ============ */

/**
 * 设置当前 Info 页面
 * @param page 页面索引
 */
void buddy_ui_set_info_page(InfoPageIdx page);

/**
 * 切换到下一页 Info
 */
void buddy_ui_info_next(void);

/**
 * 切换到上一页 Info
 */
void buddy_ui_info_prev(void);

/**
 * 获取当前 Info 页面索引
 * @return 页面索引
 */
InfoPageIdx buddy_ui_get_info_page(void);

/* ============ 菜单 ============ */

/**
 * 显示/隐藏菜单
 * @param show 是否显示
 */
void buddy_ui_show_menu(bool show);

/**
 * 判断菜单是否可见
 * @return true=可见
 */
bool buddy_ui_is_menu_visible(void);

/**
 * 选中菜单项
 * @param item 菜单项索引
 */
void buddy_ui_menu_select(BuddyMenuItem item);

/**
 * 获取当前选中的菜单项
 * @return 菜单项索引
 */
BuddyMenuItem buddy_ui_get_menu_selected(void);

/* ============ 设置 ============ */

/**
 * 显示/隐藏设置页面
 * @param show 是否显示
 */
void buddy_ui_show_settings(bool show);

/**
 * 判断设置页面是否可见
 * @return true=可见
 */
bool buddy_ui_is_settings_visible(void);

/**
 * 选中设置项
 * @param item 设置项索引
 */
void buddy_ui_settings_select(BuddySettingItem item);

/**
 * 获取当前选中的设置项
 * @return 设置项索引
 */
BuddySettingItem buddy_ui_get_settings_selected(void);

/**
 * 更新设置项的开关状态显示
 * @param item 设置项
 * @param on   开关状态
 */
void buddy_ui_settings_set_toggle(BuddySettingItem item, bool on);

/**
 * 更新亮度设置显示
 * @param pct 亮度百分比 (0-100)
 */
void buddy_ui_settings_set_brightness(int pct);

/* ============ 时钟 ============ */

/**
 * 设置时钟显示
 * @param hour   时 (0-23)
 * @param minute 分 (0-59)
 * @param second 秒 (0-59)
 */
void buddy_ui_set_clock(int hour, int minute, int second);

/* ============ BLE 配对 ============ */

/**
 * 设置 BLE 配对码
 * @param code 配对码字符串
 */
void buddy_ui_set_ble_pairing_code(const char *code);

/**
 * 显示/隐藏 BLE 配对码弹窗
 * @param show 是否显示
 */
void buddy_ui_show_ble_pairing(bool show);

/**
 * 判断 BLE 配对弹窗是否显示
 * @return true=显示中
 */
bool buddy_ui_is_ble_pairing_visible(void);

/* ============ 电池状态 ============ */

/**
 * 设置电池状态显示
 * @param percentage 电量百分比 (0-100)
 * @param charging   是否充电中
 */
void buddy_ui_set_battery(int percentage, bool charging);

/* ============ 动画 tick ============ */

/**
 * Buddy UI 动画 tick（由外部定时器调用，约 10-30Hz）
 * @param tick 全局 tick 计数
 */
void buddy_ui_anim_tick(uint32_t tick);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_UI_H */
