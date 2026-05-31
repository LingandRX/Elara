/**
 * @file test_lvgl.c
 * LVGL 集成测试
 */

#include "unity.h"
#include "lvgl.h"
#include "lvgl_chat_ui.h"
#include "lvgl_widgets.h"

TEST_CASE("lvgl_init", "[lvgl]") {
    /* 测试 LVGL 初始化 */
    lv_init();
    TEST_ASSERT_TRUE(lv_is_initialized());
}

TEST_CASE("lvgl_display_create", "[lvgl]") {
    /* 测试显示对象创建 */
    lv_init();

    lv_display_t *disp = lv_display_create(170, 320);
    TEST_ASSERT_NOT_NULL(disp);

    lv_display_delete(disp);
}

TEST_CASE("lvgl_indev_create", "[lvgl]") {
    /* 测试输入设备创建 */
    lv_init();

    lv_indev_t *indev = lv_indev_create();
    TEST_ASSERT_NOT_NULL(indev);

    lv_indev_delete(indev);
}

TEST_CASE("lvgl_label_create", "[lvgl]") {
    /* 测试标签创建 */
    lv_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *label = lv_label_create(scr);
    TEST_ASSERT_NOT_NULL(label);

    lv_label_set_text(label, "Test");
    TEST_ASSERT_EQUAL_STRING("Test", lv_label_get_text(label));

    lv_obj_delete(label);
}

TEST_CASE("lvgl_chat_ui_init", "[lvgl]") {
    /* 测试聊天界面初始化 */
    lv_init();

    /* 模拟显示和输入设备 */
    lv_display_create(170, 320);
    lv_indev_create();

    /* 初始化聊天界面 */
    lvgl_chat_ui_init();

    /* 验证容器创建 */
    lv_obj_t *container = lvgl_chat_ui_get_container();
    TEST_ASSERT_NOT_NULL(container);

    /* 清理 */
    lvgl_chat_ui_clear();
}

TEST_CASE("lvgl_chat_ui_add_msg", "[lvgl]") {
    /* 测试添加消息 */
    lv_init();

    /* 模拟显示和输入设备 */
    lv_display_create(170, 320);
    lv_indev_create();

    /* 初始化聊天界面 */
    lvgl_chat_ui_init();

    /* 添加消息 */
    lvgl_chat_ui_add_msg("user", "Hello", "happy", false);
    lvgl_chat_ui_add_msg("ai", "Hi there!", "neutral", false);
    lvgl_chat_ui_add_msg("system", "System message", "", false);

    /* 验证消息添加成功 (无崩溃) */
    TEST_PASS();
}

TEST_CASE("lvgl_chat_ui_status", "[lvgl]") {
    /* 测试状态设置 */
    lv_init();

    /* 模拟显示和输入设备 */
    lv_display_create(170, 320);
    lv_indev_create();

    /* 初始化聊天界面 */
    lvgl_chat_ui_init();

    /* 设置不同状态 */
    lvgl_chat_ui_set_status("idle", "idle");
    lvgl_chat_ui_set_status("listening", "listening");
    lvgl_chat_ui_set_status("thinking", "thinking");
    lvgl_chat_ui_set_status("replying", "replying");
    lvgl_chat_ui_set_status("error", "error");
    
    /* 设置新的 Pet 动画状态 */
    lvgl_chat_ui_set_status("run right", "happy");
    lvgl_chat_ui_set_status("waving", "happy");
    lvgl_chat_ui_set_status("failed", "sad");

    /* 验证状态设置成功 (无崩溃) */
    TEST_PASS();
}

TEST_CASE("lvgl_widgets_create", "[lvgl]") {
    /* 测试自定义控件创建 */
    lv_init();

    lv_obj_t *scr = lv_screen_active();

    /* 测试消息气泡 */
    lv_obj_t *bubble = lvgl_create_message_bubble(scr, "user", "Test message");
    TEST_ASSERT_NOT_NULL(bubble);

    /* 测试状态指示器 */
    lv_color_t color = lv_color_hex(0x00C864);
    lv_obj_t *indicator = lvgl_create_status_indicator(scr, "Test", color);
    TEST_ASSERT_NOT_NULL(indicator);

    /* 测试动画标签 */
    lv_obj_t *label = lvgl_create_animated_label(scr, "Test");
    TEST_ASSERT_NOT_NULL(label);

    /* 测试进度条 */
    lv_obj_t *bar = lvgl_create_progress_bar(scr);
    TEST_ASSERT_NOT_NULL(bar);

    /* 清理 */
    lv_obj_clean(scr);
}
