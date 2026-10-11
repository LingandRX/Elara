/**
 * @file buddy_drawer.h
 * 底部信息面板抽屉（见设计文档 §4.1 / §5.2）
 */
#ifndef BUDDY_DRAWER_H
#define BUDDY_DRAWER_H

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @param parent 挂载父容器（应为 buddy_ui_get_container()，见 R1） */
void buddy_drawer_init(lv_obj_t *parent);
/** 打开信息面板抽屉 */
void buddy_drawer_open(void);
void buddy_drawer_close(void);
bool buddy_drawer_is_open(void);
void buddy_drawer_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_DRAWER_H */
