/**
 * @file battery_monitor.h
 * 电池监测模块 - ADC读取电池电压
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化电池监测（ADC）
 */
void battery_monitor_init(void);

/**
 * @brief 获取电池电压（mV）
 * @return 电池电压，单位毫伏
 */
uint32_t battery_get_voltage_mv(void);

/**
 * @brief 获取电池百分比（0-100）
 * @return 电池电量百分比
 */
uint8_t battery_get_percentage(void);

/**
 * @brief 是否正在充电（根据电压判断）
 * @return true=可能在充电（电压较高）
 */
bool battery_is_charging(void);

/**
 * @brief 定期更新电池状态（在 main loop 中调用）
 */
void battery_monitor_update(void);

#endif /* BATTERY_MONITOR_H */
