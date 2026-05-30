/**
 * @file lvgl_example.c
 * LVGL 使用示例
 *
 * 本示例展示如何使用 LVGL 创建聊天界面
 */

#include "lvgl.h"
#include "lvgl_chat_ui.h"
#include "lvgl_widgets.h"
#include "esp_log.h"

static const char *TAG = "LVGL_EXAMPLE";

/**
 * 示例: 创建自定义界面
 */
void example_create_custom_ui(void) {
    ESP_LOGI(TAG, "Creating custom UI...");

    /* 获取活动屏幕 */
    lv_obj_t *scr = lv_screen_active();

    /* 设置背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x080810), 0);

    /* 创建标题 */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Elara Chat Demo");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    /* 创建状态指示器 */
    lv_color_t status_color = lv_color_hex(0x00C864); /* 绿色 */
    lv_obj_t *status = lvgl_create_status_indicator(scr, "空闲", status_color);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 10, 40);

    /* 创建消息气泡 */
    lv_obj_t *user_msg = lvgl_create_message_bubble(scr, "user", "你好，我是用户！");
    lv_obj_align(user_msg, LV_ALIGN_TOP_RIGHT, -10, 80);

    lv_obj_t *ai_msg = lvgl_create_message_bubble(scr, "ai", "你好！我是 Elara，有什么可以帮助你的吗？");
    lv_obj_align(ai_msg, LV_ALIGN_TOP_LEFT, 10, 140);

    /* 创建动画标签 */
    lv_obj_t *anim_label = lvgl_create_animated_label(scr, "欢迎使用 Elara");
    lv_obj_align(anim_label, LV_ALIGN_CENTER, 0, 0);

    /* 创建进度条 */
    lv_obj_t *progress = lvgl_create_progress_bar(scr);
    lv_obj_align(progress, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_bar_set_value(progress, 75, LV_ANIM_ON);

    ESP_LOGI(TAG, "Custom UI created");
}

/**
 * 示例: 使用聊天界面 API
 */
void example_use_chat_ui(void) {
    ESP_LOGI(TAG, "Using chat UI API...");

    /* 初始化聊天界面 */
    lvgl_chat_ui_init();

    /* 显示欢迎界面 */
    lvgl_chat_ui_welcome();

    /* 添加用户消息 */
    lvgl_chat_ui_add_msg("user", "今天天气怎么样？", "happy", false);

    /* 添加 AI 回复 (流式) */
    lvgl_chat_ui_add_msg("ai", "今天天气", "", true);
    lvgl_chat_ui_add_msg("ai", "很好，", "", true);
    lvgl_chat_ui_add_msg("ai", "阳光明媚！", "happy", false);

    /* 添加系统消息 */
    lvgl_chat_ui_add_msg("system", "语音识别已启用", "", false);

    /* 设置状态 */
    lvgl_chat_ui_set_status("聆听", "listening");

    ESP_LOGI(TAG, "Chat UI demo completed");
}

/**
 * 示例: 创建滚动列表
 */
void example_create_scroll_list(void) {
    ESP_LOGI(TAG, "Creating scroll list...");

    lv_obj_t *scr = lv_screen_active();

    /* 创建列表 */
    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, LV_HOR_RES - 20, LV_VER_RES - 40);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 8, 0);

    /* 添加列表项 */
    for (int i = 0; i < 10; i++) {
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, NULL);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D2D37), 0);
        lv_obj_set_style_radius(btn, 4, 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text_fmt(label, "Item %d", i + 1);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    }

    ESP_LOGI(TAG, "Scroll list created");
}

/**
 * 示例: 创建按钮矩阵
 */
void example_create_button_matrix(void) {
    ESP_LOGI(TAG, "Creating button matrix...");

    lv_obj_t *scr = lv_screen_active();

    /* 按钮文本 */
    static const char *btn_map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        LV_SYMBOL_BACKSPACE, "0", LV_SYMBOL_OK, ""
    };

    /* 创建按钮矩阵 */
    lv_obj_t *btnm = lv_btnmatrix_create(scr);
    lv_btnmatrix_set_map(btnm, btn_map);
    lv_obj_set_size(btnm, 160, 180);
    lv_obj_align(btnm, LV_ALIGN_CENTER, 0, 0);

    /* 设置样式 */
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(btnm, 0, 0);
    lv_obj_set_style_radius(btnm, 8, 0);

    ESP_LOGI(TAG, "Button matrix created");
}
