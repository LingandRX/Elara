/**
 * @file battery_monitor.c
 * 电池监测实现 - ADC读取电池电压
 * 
 * 电路：电池通过分压电阻连接到GPIO4
 * 假设使用 100K + 100K 分压，ADC参考电压 3.3V
 * 满量程 3.3V * 2 = 6.6V（足够覆盖 4.2V 锂电池）
 * 实际比例需要根据硬件调整
 */

#include "battery_monitor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "BATTERY";

/* ADC配置 */
#define BAT_ADC_CHANNEL     ADC_CHANNEL_3  /* GPIO4 对应 ADC1_CH3 */
#define BAT_ADC_UNIT        ADC_UNIT_1
#define BAT_ADC_ATTEN       ADC_ATTEN_DB_12 /* 0-3.3V 量程 */

/* 分压系数（需要根据实际硬件调整）
 * 假设使用 100K + 100K 分压：
 * Vbat = Vadc * (R1 + R2) / R2 = Vadc * 2
 */
#define VOLTAGE_DIVIDER_RATIO  2.0f

/* 锂电池电压范围 */
#define BAT_VOLTAGE_MAX     4200  /* 4.2V 满电 */
#define BAT_VOLTAGE_MIN     3300  /* 3.3V 空电（保守估计） */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali = NULL;
static bool s_cali_enabled = false;

/* 电池状态 */
static uint32_t s_voltage_mv = 0;
static uint8_t s_percentage = 0;
static bool s_charging = false;

void battery_monitor_init(void) {
    /* 配置ADC */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = BAT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BAT_ADC_CHANNEL, &chan_cfg));

    /* 配置校准 */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BAT_ADC_UNIT,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali) == ESP_OK) {
        s_cali_enabled = true;
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = BAT_ADC_UNIT,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_adc_cali) == ESP_OK) {
        s_cali_enabled = true;
    }
#endif

    ESP_LOGI(TAG, "Battery monitor initialized (cali=%s)", s_cali_enabled ? "yes" : "no");
    
    /* 首次读取 */
    battery_monitor_update();
}

void battery_monitor_update(void) {
    if (!s_adc_handle) return;

    int raw = 0;
    if (adc_oneshot_read(s_adc_handle, BAT_ADC_CHANNEL, &raw) != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed");
        return;
    }

    int voltage_mv = 0;
    if (s_cali_enabled) {
        if (adc_cali_raw_to_voltage(s_adc_cali, raw, &voltage_mv) != ESP_OK) {
            /* 校准失败，使用原始值估算 */
            voltage_mv = (raw * 3300) / 4095;
        }
    } else {
        /* 无校准：12位ADC，3.3V参考 */
        voltage_mv = (raw * 3300) / 4095;
    }

    /* 应用分压系数 */
    s_voltage_mv = (uint32_t)(voltage_mv * VOLTAGE_DIVIDER_RATIO);

    /* 计算百分比 */
    if (s_voltage_mv >= BAT_VOLTAGE_MAX) {
        s_percentage = 100;
    } else if (s_voltage_mv <= BAT_VOLTAGE_MIN) {
        s_percentage = 0;
    } else {
        s_percentage = (uint8_t)((s_voltage_mv - BAT_VOLTAGE_MIN) * 100 / 
                                   (BAT_VOLTAGE_MAX - BAT_VOLTAGE_MIN));
    }

    /* 充电检测：电压超过 4.15V 认为在充电 */
    s_charging = (s_voltage_mv > 4150);
}

uint32_t battery_get_voltage_mv(void) {
    return s_voltage_mv;
}

uint8_t battery_get_percentage(void) {
    return s_percentage;
}

bool battery_is_charging(void) {
    return s_charging;
}
