/**
 * @file test_lvgl.c
 * LVGL 集成测试
 */

#include "unity.h"
#include "lvgl.h"
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
