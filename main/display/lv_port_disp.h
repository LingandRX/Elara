/**
 * @file lv_port_disp.h
 * LVGL 显示端口头文件
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include <stdbool.h>

/* LVGL 双缓冲（RGB565 格式，2 bytes per pixel） */
extern void *lv_port_disp_buf1;
extern void *lv_port_disp_buf2;

/**
 * 初始化 LVGL 显示端口
 * @param panel LCD 面板句柄
 */
void lv_port_disp_init(esp_lcd_panel_handle_t panel);

/**
 * 设置 boot 模式（boot 测试期间跳过 LVGL flush_ready 通知）
 * @param boot true=boot 模式, false=正常模式
 */
void lv_port_disp_set_boot_mode(bool boot);

/**
 * 设置背光
 * @param on true=开启, false=关闭
 */
void lv_port_disp_set_backlight(bool on);

/**
 * 获取 LVGL 互斥锁
 * @param timeout_ms 超时时间（毫秒），-1 表示永久等待
 * @return true=获取成功, false=超时
 */
bool lv_port_disp_lock(int timeout_ms);

/**
 * 释放 LVGL 互斥锁
 */
void lv_port_disp_unlock(void);

/**
 * 通知 LVGL 刷新完成（由 on_color_trans_done 回调调用）
 */
void lv_port_disp_flush_ready(void);

#endif /* LV_PORT_DISP_H */
