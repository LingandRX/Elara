/**
 * @file main.c
 * Elara Buddy 入口点 - 从 claude-desktop-buddy 迁移
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"

#include "display/lcd_config.h"
#include "display/lv_port_disp.h"
#include "display/backlight_manager.h"
#include "input/lvgl_touch.h"
#include "ui/buddy/buddy_ui.h"
#include "ui/buddy/buddy_anim.h"
#include "ui/pet_ui.h"
#include "comm/uart_comm.h"
#include "comm/tcp_server.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "buddy/buddy_state.h"
#include "buddy/buddy_stats.h"
#include "battery/battery_monitor.h"
#include "nvs_flash.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

/* 全局设备 */
static esp_lcd_panel_handle_t panel = NULL;

/* 命令队列（与 uart_comm 共享） */
static QueueHandle_t cmdQueue;

/* 运行状态 */
static uint32_t s_tick = 0;
static uint32_t s_welcome_until_ms = 0;

/* 常量 */
#define PLAYFUL_MS          (3UL * 60UL * 1000UL)
#define SCREEN_OFF_MS       (30UL * 1000UL)
#define CLOCK_OFF_MS_BAT    (5UL * 60UL * 1000UL)
#define SPECIES_GIF         0xFF

/* SPI 颜色传输完成回调 */
static bool on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx) {
    lv_port_disp_flush_ready();
    return false;
}

/* LCD 初始化 */
static esp_err_t lcd_init(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI, .miso_io_num = -1, .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 160 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC, .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8, .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST,
                                              &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 35, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    return ESP_OK;
}

/* 应用亮度 */
static void apply_brightness(void) {
    BuddyUIState *ui = buddy_get_ui_state();
    uint8_t lvl = ui->bright_level;
    /* 0..4 → 亮度映射 */
    uint8_t brightness = (lvl * 45);  /* 0, 45, 90, 135, 180 (~70% max) */
    if (brightness > 179) brightness = 179;
    backlight_set_brightness(brightness);
}

/* 唤醒 */
static void wake(void) {
    BuddyRuntime *rt = buddy_get_runtime();
    rt->last_interact_ms = esp_timer_get_time() / 1000;
    if (rt->screen_off) {
        rt->screen_off = false;
        rt->wake_transition_until_ms = rt->last_interact_ms + 12000;
    }
    if (rt->dimmed) {
        apply_brightness();
        rt->dimmed = false;
    }
}

/* 发送命令到上位机 */
static void send_cmd(const char *json) {
    uart_send_raw(json, strlen(json));
    uart_send_raw("\n", 1);
}

/* 提示音（禁用，因为没有音频硬件） */
static void beep(uint16_t freq, uint16_t dur) {
    (void)freq;
    (void)dur;
    /* 无音频硬件，静默 */
}

/* 时间同步更新 UI */
static void update_clock(void) {
    int h = 0, m = 0, s = 0;
    if (buddy_get_local_time(&h, &m, &s, NULL, NULL, NULL, NULL)) {
        buddy_ui_set_clock(h, m, s);
    }
}

/* LVGL 处理任务 */
static void lvgl_handler_task(void *pvParam) {
    (void)pvParam;
    while (1) {
        uint32_t delay = 500;
        if (lv_port_disp_lock(-1)) {
            delay = lv_task_handler();
            lv_port_disp_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(delay > 500 ? 500 : (delay < 1 ? 1 : delay)));
    }
}

/* UART 命令处理任务 */
static void cmd_process_task(void *pvParam) {
    (void)pvParam;
    UartCmd cmd;
    while (1) {
        if (xQueueReceive(cmdQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* petdex 命令: 打开 Petdex 动画页并切换状态 */
            if (cmd.type == CMD_PETDEX) {
                if (lv_port_disp_lock(-1)) {
                    pet_ui_set_state_by_name(cmd.state);
                    pet_ui_show(true);
                    lv_port_disp_unlock();
                }
            }
        }
    }
}

/* 前向声明（定义在下方） */
static void process_boot_key(void);

/* 主循环 - 核心状态机（直接内联到 app_main，不创建独立任务） */
static void buddy_main_loop(void) {
    BuddyRuntime *rt = buddy_get_runtime();
    BuddyUIState *ui = buddy_get_ui_state();
    ClaudeState *claude = buddy_get_claude_state();

    static bool was_clocking = false;

    s_tick++;
    uint32_t now_ms = esp_timer_get_time() / 1000;

    /* 1. 轮询数据 */
    buddy_data_poll(claude);

    /* 2. 检查升级 */
    if (buddy_stats_poll_level_up()) {
        buddy_trigger_oneshot(PERSONA_CELEBRATE, 3000);
    }

    /* 3. 推导基础状态 */
    rt->base_state = buddy_derive_state(claude);

    /* 4. 唤醒过渡保护 */
    if (rt->base_state == PERSONA_IDLE &&
        (int32_t)(now_ms - rt->wake_transition_until_ms) < 0) {
        /* 保持 idle */
    }

    /* 5. One-shot 超时 */
    if ((int32_t)(now_ms - rt->oneshot_until_ms) >= 0) {
        rt->active_state = rt->base_state;
    }

    /* 6. 新审批提示到达 */
    if (strcmp(claude->prompt_id, rt->last_prompt_id) != 0) {
        strncpy(rt->last_prompt_id, claude->prompt_id, sizeof(rt->last_prompt_id) - 1);
        rt->last_prompt_id[sizeof(rt->last_prompt_id) - 1] = '\0';
        rt->response_sent = false;
        if (claude->prompt_id[0]) {
            rt->prompt_arrived_ms = now_ms;
            wake();
            beep(1200, 80);
            ui->display_mode = DISP_NORMAL;
            ui->menu_open = false;
            ui->settings_open = false;
            ui->reset_open = false;
            buddy_ui_hide_approval();
            buddy_ui_show_approval(claude->prompt_tool, claude->prompt_hint);
        } else if (!rt->response_sent) {
            /* 审批被撤销 */
            buddy_ui_hide_approval();
        }
    }

    /* 7. 处理审批计时 */
    bool in_prompt = claude->prompt_id[0] && !rt->response_sent;
    if (in_prompt && !buddy_ui_is_approval_visible()) {
        /* 确保审批界面显示 */
        buddy_ui_show_approval(claude->prompt_tool, claude->prompt_hint);
    }

    /* 8. 推导 clocking 状态 */
    bool clocking = claude->connected &&
                    claude->sessions_running == 0 && claude->sessions_waiting == 0 &&
                    buddy_rtc_valid();

    if (clocking != was_clocking) {
        if (clocking) {
            buddy_anim_set_peek(true);
        } else {
            buddy_anim_set_peek(false);
        }
        buddy_anim_invalidate();
        was_clocking = clocking;
    }

    /* 9. 更新时钟显示 */
    update_clock();

    /* 10. 时钟模式下的时间心情 */
    if (clocking && (int32_t)(now_ms - rt->oneshot_until_ms) >= 0) {
        if ((int32_t)(now_ms - rt->playful_until_ms) < 0) {
            /*  playful 模式 */
            static const PersonaState PLAYFUL[] = {
                PERSONA_IDLE, PERSONA_IDLE, PERSONA_HEART,
                PERSONA_IDLE, PERSONA_CELEBRATE, PERSONA_IDLE
            };
            rt->active_state = PLAYFUL[(now_ms / 5000) % 6];
        } else {
            /* 时间节律 */
            int h = 0;
            buddy_get_local_time(&h, NULL, NULL, NULL, NULL, NULL, NULL);
            if (h < 7 || h >= 22) {
                rt->active_state = (now_ms / 15000 % 8 == 0) ? PERSONA_IDLE : PERSONA_SLEEP;
            } else {
                rt->active_state = (now_ms / 12000 % 6 == 0) ? PERSONA_SLEEP : PERSONA_IDLE;
            }
        }
    }

    /* 11. 电池监测（每10秒更新一次） */
    static uint32_t last_bat_update = 0;
    if (now_ms - last_bat_update >= 10000) {
        last_bat_update = now_ms;
        battery_monitor_update();
        buddy_ui_set_battery(battery_get_percentage(), battery_is_charging());
    }

    /* 12. 渲染更新 */
    if (!rt->napping && !rt->screen_off) {
        if (lv_port_disp_lock(50)) {
            /* 更新动画 */
            buddy_anim_tick(rt->active_state, s_tick);
            buddy_ui_anim_tick(s_tick);

            /* 更新 HUD（欢迎消息期间不覆盖） */
            if ((int32_t)(now_ms - s_welcome_until_ms) >= 0) {
                if (claude->n_lines > 0) {
                    buddy_ui_set_hud_text(claude->lines[claude->n_lines - 1]);
                } else {
                    buddy_ui_set_hud_text(claude->msg);
                }
            }

            /* 更新宠物统计 */
            BuddyStats *stats = buddy_stats_get();
            buddy_ui_set_pet_stats(
                buddy_stats_mood_tier() * 25,
                buddy_stats_fed_progress() * 10,
                buddy_stats_energy_tier() * 20,
                stats->level
            );

            lv_port_disp_unlock();
        }
    }

    /* 12. 自动休眠（简化：无 IMU，仅基于超时） */
    BuddySettings *settings = buddy_settings_get();
    if (settings->auto_sleep && !rt->screen_off && !in_prompt) {
        uint32_t idle_ms = now_ms - rt->last_interact_ms;
        uint32_t threshold = clocking ? CLOCK_OFF_MS_BAT : SCREEN_OFF_MS;
        if (idle_ms > threshold) {
            backlight_set_on(false);
            rt->screen_off = true;
        }
    }

    /* 13. LTPO-lite 动态帧率 */
    uint32_t loop_ms;
    if (rt->screen_off) {
        loop_ms = 200;
    } else if (rt->napping || in_prompt || ui->menu_open ||
               ui->settings_open || ui->reset_open ||
               (int32_t)(now_ms - rt->oneshot_until_ms) < 0) {
        loop_ms = 16;
    } else {
        loop_ms = 100;
    }

    /* 在主循环中处理 BOOT 键，减少任务数量 */
    process_boot_key();

    vTaskDelay(pdMS_TO_TICKS(loop_ms));
}



/* BOOT 键状态 */
static uint8_t bk_last_level = 1;
static uint32_t bk_press_start_ms = 0;

/* 执行菜单项动作（进入对应子菜单）
 * 由 BOOT 键短按确认和触摸点击菜单项共同触发 */
static void execute_menu_action(BuddyMenuItem item) {
    BuddyUIState *ui = buddy_get_ui_state();
    BuddyRuntime *rt = buddy_get_runtime();

    ui->menu_open = false;
    buddy_ui_show_menu(false);

    switch (item) {
    case BUDDY_MENU_SETTINGS:
        ui->settings_open = true;
        ui->settings_sel = 0;
        buddy_ui_show_settings(true);
        buddy_ui_settings_select(BUDDY_SET_BRIGHTNESS);
        break;
    case BUDDY_MENU_SHUTDOWN:
        ESP_LOGI(TAG, "Shutdown requested");
        backlight_set_on(false);
        rt->screen_off = true;
        esp_sleep_enable_ext0_wakeup(BOOT_KEY_PIN, 0);
        esp_deep_sleep_start();
        break;
    case BUDDY_MENU_HELP:
        buddy_ui_set_hud_text("Short=nav  Long=back");
        buddy_ui_set_hud_visible(true);
        break;
    case BUDDY_MENU_ABOUT:
        ui->display_mode = DISP_INFO;
        ui->info_page = INFO_PAGE_ABOUT;
        buddy_ui_set_mode(BUDDY_MODE_INFO);
        buddy_ui_set_info_page(INFO_PAGE_ABOUT);
        break;
    case BUDDY_MENU_DEMO:
        buddy_set_demo(!buddy_is_demo());
        buddy_ui_set_hud_text(buddy_is_demo() ? "Demo ON" : "Demo OFF");
        buddy_ui_set_hud_visible(true);
        buddy_anim_invalidate();
        break;
    case BUDDY_MENU_CLOSE:
    default:
        buddy_anim_invalidate();
        break;
    }
}

/* 触摸点击菜单项回调（运行于 LVGL 锁内） */
static void on_menu_action(BuddyMenuItem item) {
    BuddyUIState *ui = buddy_get_ui_state();
    /* 同步选中状态，保持与物理按键导航一致 */
    ui->menu_sel = (uint8_t)item;
    /* LVGL 事件回调已持有 LVGL 锁，直接执行动作 */
    execute_menu_action(item);
}

/* 执行设置项动作（切换开关/调节亮度/进入 Reset/返回）
 * 由 BOOT 键长按确认和触摸点击设置项共同触发 */
static void execute_settings_action(BuddySettingItem item) {
    BuddyUIState *ui = buddy_get_ui_state();

    switch (item) {
    case BUDDY_SET_BRIGHTNESS: {
        ui->bright_level = (ui->bright_level + 1) % 5;
        apply_brightness();
        buddy_ui_settings_set_brightness(ui->bright_level * 25);
        beep(1800, 30);
        break;
    }
    case BUDDY_SET_SOUND: {
        BuddySettings *set = buddy_settings_get();
        set->sound = !set->sound;
        buddy_ui_settings_set_toggle(BUDDY_SET_SOUND, set->sound);
        if (set->sound) beep(2000, 50);
        buddy_settings_save();
        break;
    }
    case BUDDY_SET_WIFI: {
        BuddySettings *set = buddy_settings_get();
        set->wifi = !set->wifi;
        buddy_ui_settings_set_toggle(BUDDY_SET_WIFI, set->wifi);
        buddy_settings_save();
        break;
    }
    case BUDDY_SET_LED: {
        BuddySettings *set = buddy_settings_get();
        set->led = !set->led;
        buddy_ui_settings_set_toggle(BUDDY_SET_LED, set->led);
        buddy_settings_save();
        break;
    }
    case BUDDY_SET_HUD: {
        BuddySettings *set = buddy_settings_get();
        set->hud = !set->hud;
        buddy_ui_settings_set_toggle(BUDDY_SET_HUD, set->hud);
        buddy_ui_set_hud_visible(set->hud);
        buddy_settings_save();
        break;
    }
    case BUDDY_SET_ROTATE: {
        BuddySettings *set = buddy_settings_get();
        set->clock_rot = (set->clock_rot + 1) % 3;
        buddy_settings_save();
        beep(1800, 30);
        break;
    }
    case BUDDY_SET_ASCII: {
        ui->buddy_mode = !ui->buddy_mode;
        buddy_ui_settings_set_toggle(BUDDY_SET_ASCII, ui->buddy_mode);
        if (ui->buddy_mode) buddy_anim_set_species_idx(0);
        break;
    }
    case BUDDY_SET_AUTO_SLEEP: {
        BuddySettings *set = buddy_settings_get();
        set->auto_sleep = !set->auto_sleep;
        buddy_ui_settings_set_toggle(BUDDY_SET_AUTO_SLEEP, set->auto_sleep);
        buddy_settings_save();
        beep(1800, 30);
        break;
    }
    case BUDDY_SET_RESET: {
        ui->settings_open = false;
        buddy_ui_show_settings(false);
        ui->reset_open = true;
        ui->reset_sel = 0;
        ui->reset_confirm_idx = 0xFF;
        break;
    }
    case BUDDY_SET_BACK: {
        ui->settings_open = false;
        buddy_ui_show_settings(false);
        buddy_anim_invalidate();
        break;
    }
    default: break;
    }
}

/* 触摸点击设置项回调（运行于 LVGL 锁内） */
static void on_settings_action(BuddySettingItem item) {
    BuddyUIState *ui = buddy_get_ui_state();
    /* 同步选中状态，保持与物理按键导航一致 */
    ui->settings_sel = (uint8_t)item;
    /* LVGL 事件回调已持有 LVGL 锁，直接执行动作 */
    execute_settings_action(item);
}

/* 执行审批动作（批准/拒绝）
 * 由 BOOT 键（短按=批准 / 长按=拒绝）和触摸点击审批按钮共同触发 */
static void execute_approval_action(bool approve) {
    BuddyRuntime *rt = buddy_get_runtime();
    if (!buddy_has_pending_prompt()) {
        /* 无待审批项，仅关闭弹窗 */
        buddy_ui_hide_approval();
        return;
    }
    char cmd[96];
    snprintf(cmd, sizeof(cmd),
             "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}",
             buddy_get_claude_state()->prompt_id, approve ? "once" : "deny");
    send_cmd(cmd);
    rt->response_sent = true;
    if (approve) {
        uint32_t now_ms = esp_timer_get_time() / 1000;
        uint32_t took_s = (now_ms - rt->prompt_arrived_ms) / 1000;
        buddy_stats_on_approval(took_s);
        beep(2400, 60);
        if (took_s < 5) buddy_trigger_oneshot(PERSONA_HEART, 2000);
    } else {
        buddy_stats_on_denial();
        beep(1000, 80);
        buddy_trigger_oneshot(PERSONA_DIZZY, 2000);
    }
    buddy_ui_hide_approval();
}

/* 触摸点击审批按钮回调（运行于 LVGL 锁内） */
static void on_approval_action(bool approve) {
    /* LVGL 事件回调已持有 LVGL 锁，直接执行动作 */
    execute_approval_action(approve);
}

static void process_boot_key(void) {
    BuddyUIState *ui = buddy_get_ui_state();
    BuddyRuntime *rt = buddy_get_runtime();
    uint8_t level = gpio_get_level(BOOT_KEY_PIN);
    uint32_t now_ms = esp_timer_get_time() / 1000;

    if (bk_last_level == 1 && level == 0) {
        /* 按下 */
        ESP_LOGI(TAG, "BOOT key pressed");
        bk_press_start_ms = now_ms;
        if (rt->screen_off) rt->swallow_btn_a = true;
        wake();
    }

    if (bk_last_level == 0 && level == 1) {
        /* 释放 */
        uint32_t press_dur = now_ms - bk_press_start_ms;
        ESP_LOGI(TAG, "BOOT key released, dur=%lu ms", press_dur);

        /* Petdex 页面: 按 BOOT 返回主界面 */
        if (pet_ui_is_visible()) {
            if (lv_port_disp_lock(-1)) {
                pet_ui_show(false);
                lv_port_disp_unlock();
            }
            return;
        }

        if (rt->swallow_btn_a) {
            rt->swallow_btn_a = false;
        } else if (press_dur >= 600) {
            /* 长按 */
            beep(800, 60);
            if (buddy_has_pending_prompt()) {
                execute_approval_action(false);
            } else if (ui->reset_open) {
                ui->reset_open = false;
            } else if (ui->settings_open) {
                execute_settings_action((BuddySettingItem)ui->settings_sel);
            } else if (ui->menu_open) {
                ui->menu_open = false;
                buddy_ui_show_menu(false);
                buddy_anim_invalidate();
            } else {
                ui->menu_open = true;
                ui->menu_sel = 0;
                buddy_ui_show_menu(true);
            }
        } else {
            /* 短按 */
            beep(1800, 30);
            if (buddy_has_pending_prompt()) {
                execute_approval_action(true);
            } else if (ui->reset_open) {
                ui->reset_sel = (ui->reset_sel + 1) % 3;
                ui->reset_confirm_idx = 0xFF;
            } else if (ui->settings_open) {
                ui->settings_sel = (ui->settings_sel + 1) % BUDDY_SET_MAX;
                buddy_ui_settings_select((BuddySettingItem)ui->settings_sel);
            } else if (ui->menu_open) {
                execute_menu_action((BuddyMenuItem)ui->menu_sel);
            } else {
                ui->display_mode = (ui->display_mode + 1) % DISP_COUNT;
                if (ui->display_mode == DISP_NORMAL) buddy_ui_set_mode(BUDDY_MODE_NORMAL);
                else if (ui->display_mode == DISP_PET) buddy_ui_set_mode(BUDDY_MODE_PET);
                else buddy_ui_set_mode(BUDDY_MODE_INFO);
            }
        }
    }
    bk_last_level = level;
}

/* 覆盖层点击外部关闭回调 */
static void on_menu_closed(void) {
    BuddyUIState *ui = buddy_get_ui_state();
    ui->menu_open = false;
}

static void on_settings_closed(void) {
    BuddyUIState *ui = buddy_get_ui_state();
    ui->settings_open = false;
}

static void on_approval_closed(void) {
    BuddyRuntime *rt = buddy_get_runtime();
    rt->response_sent = true;
    buddy_stats_on_denial();
}

void app_main(void) {
    ESP_LOGI(TAG, "Elara Buddy Starting...");

    /* 0. 初始化 NVS（Buddy Stats 等模块依赖 NVS） */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 1. 初始化 LCD */
    ESP_ERROR_CHECK(lcd_init());

    /* 2. 初始化 LVGL */
    lv_init();
    lv_extra_init();
    lv_port_disp_init(panel);
    lvgl_touch_init();

    /* 3. 初始化 Buddy UI */
    buddy_ui_init();
    buddy_ui_set_overlay_close_cb(on_menu_closed, on_settings_closed, on_approval_closed);
    buddy_ui_set_menu_action_cb(on_menu_action);
    buddy_ui_set_settings_action_cb(on_settings_action);
    buddy_ui_set_approval_action_cb(on_approval_action);
    buddy_ui_show(true);

    /* 4. 初始化存储 */
    storage_init();
    storage_init_pet_dirs();

    /* 4.5 初始化 Petdex 动画页面 (需在 storage_init 之后) */
    pet_ui_init();

    /* 5. 初始化 Buddy 核心 */
    buddy_state_init();
    buddy_stats_init();
    buddy_settings_init();
    pet_name_load();
    owner_name_load();

    /* 6. 初始化角色动画 */
    buddy_anim_init();

    /* 6.5 初始化电池监测 */
    battery_monitor_init();

    /* 7. 创建命令队列与通信 */
    cmdQueue = xQueueCreate(16, sizeof(UartCmd));
    uart_comm_init(&cmdQueue);

    /* 7.5 尽早创建任务，避免后续 WiFi 占用内部 RAM 后失败 */
    BaseType_t task_ret;
    task_ret = xTaskCreate(lvgl_handler_task, "lvgl_handler", 8192, NULL, 2, NULL);
    if (task_ret != pdPASS) ESP_LOGE(TAG, "Failed to create lvgl_handler task");
    task_ret = xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) ESP_LOGE(TAG, "Failed to create uart_rx task");
    task_ret = xTaskCreate(cmd_process_task, "cmd_proc", 3072, NULL, 5, NULL);
    if (task_ret != pdPASS) ESP_LOGE(TAG, "Failed to create cmd_proc task");

    /* 8. 应用设置 */
    BuddySettings *set = buddy_settings_get();
    buddy_ui_settings_set_toggle(BUDDY_SET_AUTO_SLEEP, set->auto_sleep);

    /* 8.5 启动 WiFi 与 TCP Server（上位机网络通信，强制开启） */
    wifi_manager_init();
    tcp_server_init();

    /* 10. 应用亮度（禁用 backlight_manager 的自动休眠，由 buddy_main_task 统一管理） */
    apply_brightness();
    backlight_manager_init(179);
    backlight_enable_auto_sleep(false, 0, 0);  /* 禁用，避免与 buddy_main_task 冲突 */
    backlight_set_on(true);

    /* 11. 加载物种设置 */
    BuddyUIState *ui = buddy_get_ui_state();
    uint8_t species_idx = species_idx_load();
    ui->buddy_mode = true;
    if (species_idx == SPECIES_GIF) {
        /* 检查是否有 GIF 角色 */
        ui->buddy_mode = false;
    } else {
        buddy_anim_set_species_idx(species_idx % buddy_anim_get_species_count());
    }

    /* 12. 显示欢迎界面 */
    if (lv_port_disp_lock(-1)) {
        if (owner_name_get()[0]) {
            char line[40];
            snprintf(line, sizeof(line), "%s's", owner_name_get());
            buddy_ui_set_hud_text(line);
        } else {
            buddy_ui_set_hud_text("Hello!");
        }
        buddy_ui_set_hud_visible(true);
        lv_port_disp_unlock();
    }
    s_welcome_until_ms = esp_timer_get_time() / 1000 + 5000; /* 欢迎消息显示 5 秒 */

    /* 14. 配置 BOOT 键 GPIO */
    gpio_reset_pin(BOOT_KEY_PIN);
    gpio_set_direction(BOOT_KEY_PIN, GPIO_MODE_INPUT);
    gpio_pullup_en(BOOT_KEY_PIN);

    ESP_LOGI(TAG, "Buddy mode: %s", ui->buddy_mode ? "ASCII" : "GIF");
    ESP_LOGI(TAG, "Main loop starting (buddy+boot_key merged)");

    /* 15. 主循环（buddy_main_loop 内部已包含 boot_key + vTaskDelay） */
    while (1) {
        buddy_main_loop();
    }
}
