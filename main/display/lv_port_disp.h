/**
 * @file lv_port_disp.h
 * LVGL 显示端口头文件
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#include "sh8601.h"

/**
 * 初始化 LVGL 显示端口
 * @param lcd SH8601 LCD 设备指针
 */
void lv_port_disp_init(sh8601_dev_t *lcd);

#endif /* LV_PORT_DISP_H */
