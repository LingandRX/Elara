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
#include "esp_mac.h"
#include "esp_timer.h"
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
#include "comm/uart_comm.h"
#include "comm/ble_bridge.h"
#include "comm/tcp_server.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "buddy/buddy_state.h"
#include "buddy/buddy_stats.h"
#include "nvs_flash.h"
#include "lvgl.h"

static const char *TAG = "MAIN";

/* 全局设备 */
static esp_lcd_panel_handle_t panel = NULL;

/* 命令队列（与 uart_comm 共享） */
static QueueHandle_t cmdQueue;

/* 运行状态 */
static uint32_t s_tick = 0;
static char s_bt_name[24] = "Claude";

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

/* 启动 BLE */
static void start_ble(void) {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_bt_name, sizeof(s_bt_name), "Claude-%02X%02X", mac[4], mac[5]);
    ble_init(s_bt_name);
    ESP_LOGI(TAG, "BLE advertising as '%s'", s_bt_name);
}

/* 应用亮度 */
static void apply_brightness(void) {
    BuddyUIState *ui = buddy_get_ui_state();
    uint8_t lvl = ui->bright_level;
    /* 0..4 → 亮度映射 */
    uint8_t brightness = (lvl * 51);  /* 0, 51, 102, 153, 204 */
    if (brightness > 200) brightness = 200;
    backlight_set_brightness(brightness);
}

/* 唤醒 */
static void wake(void) {
    BuddyRuntime *rt = buddy_get_runtime();
    rt->last_interact_ms = esp_timer_get_time() / 1000;
    if (rt->screen_off) {
        backlight_set_on(true);
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
    ble_write((const uint8_t *)json, strlen(json));
    ble_write((const uint8_t *)"\n", 1);
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
            /* 现有命令通过 buddy_state 处理 */
            /* 这里可以添加对特殊命令的处理 */
        }
    }
}

/* 主循环任务 - 核心状态机 */
static void buddy_main_task(void *pvParam) {
    (void)pvParam;

    BuddyRuntime *rt = buddy_get_runtime();
    BuddyUIState *ui = buddy_get_ui_state();
    ClaudeState *claude = buddy_get_claude_state();

    uint32_t last_passkey = 0;
    static bool was_clocking = false;

    while (1) {
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
                buddy_anim_invalidate();
            } else {
                buddy_ui_hide_approval();
            }
        }

        bool in_prompt = buddy_has_pending_prompt();

        /* 7. 处理触摸输入 */
        /* 触摸处理由 lvgl_touch 在 lv_task_handler 中处理 */
        /* 这里处理高级手势逻辑 */

        /* 8. 时钟模式 */
        update_clock();
        bool clocking = (ui->display_mode == DISP_NORMAL) &&
                        !ui->menu_open && !ui->settings_open && !ui->reset_open &&
                        !in_prompt &&
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

        /* 9. 时钟模式下的时间心情 */
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

        /* 10. BLE 配对码 */
        uint32_t pk = ble_passkey();
        if (pk && !last_passkey) {
            wake();
            beep(1800, 60);
            char code[16];
            snprintf(code, sizeof(code), "%06lu", (unsigned long)pk);
            buddy_ui_set_ble_pairing_code(code);
            buddy_ui_show_ble_pairing(true);
        }
        if (!pk && last_passkey) {
            buddy_ui_show_ble_pairing(false);
        }
        last_passkey = pk;

        /* 11. 渲染更新 */
        if (!rt->napping && !rt->screen_off) {
            if (lv_port_disp_lock(50)) {
                /* 更新动画 */
                buddy_anim_tick(rt->active_state, s_tick);
                buddy_ui_anim_tick(s_tick);

                /* 更新 HUD */
                if (claude->n_lines > 0) {
                    buddy_ui_set_hud_text(claude->lines[claude->n_lines - 1]);
                } else {
                    buddy_ui_set_hud_text(claude->msg);
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
        if (!rt->screen_off && !in_prompt) {
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
                   (int32_t)(now_ms - rt->oneshot_until_ms) < 0 || pk) {
            loop_ms = 16;
        } else {
            loop_ms = 100;
        }

        vTaskDelay(pdMS_TO_TICKS(loop_ms));
    }
}

/* BOOT 键处理任务 */
static void boot_key_task(void *pvParam) {
    (void)pvParam;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_KEY_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    BuddyUIState *ui = buddy_get_ui_state();
    BuddyRuntime *rt = buddy_get_runtime();
    uint8_t last_level = 1;
    uint32_t press_start_ms = 0;

    while (1) {
        uint8_t level = gpio_get_level(BOOT_KEY_PIN);
        uint32_t now_ms = esp_timer_get_time() / 1000;

        if (last_level == 1 && level == 0) {
            /* 按下 */
            press_start_ms = now_ms;
            if (rt->screen_off) {
                rt->swallow_btn_a = true;
            }
            wake();
        }

        if (last_level == 0 && level == 1) {
            /* 释放 */
            uint32_t press_dur = now_ms - press_start_ms;

            if (rt->swallow_btn_a) {
                rt->swallow_btn_a = false;
            } else if (press_dur >= 600) {
                /* 长按：菜单 */
                beep(800, 60);
                if (ui->reset_open) {
                    ui->reset_open = false;
                } else if (ui->settings_open) {
                    ui->settings_open = false;
                    buddy_anim_invalidate();
                } else {
                    ui->menu_open = !ui->menu_open;
                    ui->menu_sel = 0;
                    buddy_ui_show_menu(ui->menu_open);
                    if (!ui->menu_open) buddy_anim_invalidate();
                }
            } else {
                /* 短按 */
                beep(1800, 30);
                if (buddy_has_pending_prompt()) {
                    /* 批准 */
                    char cmd[96];
                    snprintf(cmd, sizeof(cmd),
                             "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"once\"}",
                             buddy_get_claude_state()->prompt_id);
                    send_cmd(cmd);
                    rt->response_sent = true;
                    uint32_t took_s = (now_ms - rt->prompt_arrived_ms) / 1000;
                    buddy_stats_on_approval(took_s);
                    beep(2400, 60);
                    if (took_s < 5) buddy_trigger_oneshot(PERSONA_HEART, 2000);
                    buddy_ui_hide_approval();
                } else if (ui->reset_open) {
                    ui->reset_sel = (ui->reset_sel + 1) % 3;
                    ui->reset_confirm_idx = 0xFF;
                } else if (ui->settings_open) {
                    ui->settings_sel = (ui->settings_sel + 1) % 10;
                    buddy_ui_settings_select((BuddySettingItem)ui->settings_sel);
                } else if (ui->menu_open) {
                    ui->menu_sel = (ui->menu_sel + 1) % 6;
                    buddy_ui_menu_select((BuddyMenuItem)ui->menu_sel);
                } else {
                    /* 切换显示模式 */
                    ui->display_mode = (ui->display_mode + 1) % DISP_COUNT;
                    if (ui->display_mode == DISP_NORMAL) {
                        buddy_ui_set_mode(BUDDY_MODE_NORMAL);
                    } else if (ui->display_mode == DISP_PET) {
                        buddy_ui_set_mode(BUDDY_MODE_PET);
                    } else {
                        buddy_ui_set_mode(BUDDY_MODE_INFO);
                    }
                }
            }
        }

        last_level = level;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* BLE 数据轮询任务 */
static void ble_poll_task(void *pvParam) {
    (void)pvParam;
    while (1) {
        while (ble_available()) {
            int c = ble_read();
            if (c < 0) break;
            /* 通过 buddy_feed_ble_data 处理 */
            uint8_t ch = (uint8_t)c;
            buddy_feed_ble_data(&ch, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Elara Buddy Starting...");

    /* 0. 初始化 NVS（BLE、Buddy Stats 等模块依赖 NVS） */
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
    buddy_ui_show(true);

    /* 4. 初始化存储 */
    storage_init();
    storage_init_pet_dirs();

    /* 5. 初始化 Buddy 核心 */
    buddy_state_init();
    buddy_stats_init();
    buddy_settings_init();
    pet_name_load();
    owner_name_load();

    /* 6. 初始化角色动画 */
    buddy_anim_init();

    /* 7. 创建命令队列与通信 */
    cmdQueue = xQueueCreate(16, sizeof(UartCmd));
    uart_comm_init(&cmdQueue);

    /* 8. 初始化 Wi-Fi/TCP */
    wifi_manager_init();
    tcp_server_init();

    /* 9. 启动 BLE */
    start_ble();

    /* 10. 应用亮度 */
    apply_brightness();
    backlight_manager_init(200);
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
        lv_port_disp_unlock();
    }

    /* 13. 创建任务 */
    xTaskCreate(lvgl_handler_task, "lvgl_handler", 8192, NULL, 2, NULL);
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(cmd_process_task, "cmd_proc", 4096, NULL, 5, NULL);
    xTaskCreate(buddy_main_task, "buddy_main", 8192, NULL, 3, NULL);
    xTaskCreate(boot_key_task, "boot_key", 4096, NULL, 1, NULL);
    xTaskCreate(ble_poll_task, "ble_poll", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Buddy mode: %s", ui->buddy_mode ? "ASCII" : "GIF");

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
