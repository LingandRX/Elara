/**
 * @file buddy_ui.h
 * Buddy 主页面 UI
 * Buddy 主界面
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
    BUDDY_MODE_INFO,
    BUDDY_MODE_MAX
} BuddyMode;

/* ============ Info 页面索引 ============ */
typedef enum {
    INFO_PAGE_ABOUT = 0,
    INFO_PAGE_MAX
} InfoPageIdx;

/* ============ 菜单项 ============ */
typedef enum {
    BUDDY_MENU_WIFI = 0,
    BUDDY_MENU_SETTINGS,
    BUDDY_MENU_ABOUT,
    BUDDY_MENU_SHUTDOWN,
    BUDDY_MENU_MAX
} BuddyMenuItem;

/* ============ 设置项 ============ */
typedef enum {
    BUDDY_SET_BRIGHTNESS = 0,
    BUDDY_SET_INFO_PANEL,   /* 信息面板开关（原 HUD） */
    BUDDY_SET_ROTATE,
    BUDDY_SET_WIFI,
    BUDDY_SET_SOUND,
    BUDDY_SET_AUTO_SLEEP,
    BUDDY_SET_RESET,
    BUDDY_SET_BACK,
    BUDDY_SET_MAX
} BuddySettingItem;

/* ============ 外部状态同步回调 ============ */
typedef void (*BuddyOverlayCloseCb)(void);

/* 菜单项点击动作回调（触摸点击菜单项时触发，用于进入子菜单） */
typedef void (*BuddyMenuActionCb)(BuddyMenuItem item);

/* 设置项点击动作回调（触摸点击设置项时触发，用于执行对应动作） */
typedef void (*BuddySettingActionCb)(BuddySettingItem item);

/**
 * 注册覆盖层点击外部时的关闭回调
 * 用于同步 main.c 中的 BuddyUIState 状态
 */
void buddy_ui_set_overlay_close_cb(BuddyOverlayCloseCb menu_cb,
                                    BuddyOverlayCloseCb settings_cb);

/**
 * 注册菜单项点击动作回调
 * 触摸点击菜单项时触发，用于执行对应动作（进入子菜单）
 */
void buddy_ui_set_menu_action_cb(BuddyMenuActionCb cb);

/**
 * 注册设置项点击动作回调
 * 触摸点击设置项时触发，用于执行对应动作
 */
void buddy_ui_set_settings_action_cb(BuddySettingActionCb cb);

/* ============ 初始化与显示 ============ */

/**
 * 处理触摸滑动（由 lvgl_touch 驱动层回调）
 * @param dir 滑动方向
 */
void buddy_ui_handle_swipe(lv_dir_t dir);

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
 * 更新主界面 Persona 状态词与心情指示
 * @param persona_state PersonaState 枚举值
 */
void buddy_ui_set_persona(uint8_t persona_state);

/* ============ INFO 页面 ============ */

/**
 * 设置当前 Info 页面
 * @param page 页面索引
 */
void buddy_ui_set_info_page(InfoPageIdx page);

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

/**
 * 更新顶部 WiFi 连接标识
 * @param enabled   WiFi 功能是否开启（设置中关闭时不显示）
 * @param signal_level 信号强度等级：0=未连接/极弱, 1=弱, 2=中, 3=强
 */
void buddy_ui_set_wifi(bool enabled, int signal_level);

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
