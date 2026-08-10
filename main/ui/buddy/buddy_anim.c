/**
 * @file buddy_anim.c
 * Buddy ASCII 角色动画系统实现
 * 18 种角色 × 7 种状态，使用 LVGL v8 lv_label 绘制 ASCII 艺术
 */

#include "buddy_anim.h"
#include <string.h>

#define BUDDY_COUNT 18

/* 全局 tick 计数 */
uint32_t g_buddy_tick = 0;

/* 内部状态 */
static lv_obj_t *s_buddy_label = NULL;
static lv_obj_t *s_current_parent = NULL;
static uint8_t s_current_species = 0;
static bool s_peek = false;
static lv_coord_t s_cx = 0;
static lv_coord_t s_cy = 0;

/* ==========================================================================
 * 辅助函数
 * ========================================================================== */

/**
 * 根据 tick 和状态计算简单的动画垂直偏移
 */
static lv_coord_t buddy_anim_offset(uint32_t tick, uint8_t state)
{
    switch (state) {
    case 0: /* sleep - 缓慢呼吸 */
        return (lv_coord_t)((tick / 8) % 2);
    case 2: /* busy - 小幅度抖动 */
        return (lv_coord_t)((tick / 4) % 2);
    case 4: /* celebrate - 弹跳 */
        return (lv_coord_t)(-((tick / 6) % 3));
    case 5: /* dizzy - 摇晃 */
        return (lv_coord_t)(((tick / 3) % 2) ? 1 : -1);
    default:
        return 0;
    }
}

/**
 * 获取或创建 canvas 上的 buddy 标签
 */
static lv_obj_t *buddy_ensure_label(lv_obj_t *parent)
{
    if (s_buddy_label && s_current_parent == parent) {
        return s_buddy_label;
    }
    if (s_buddy_label) {
        lv_obj_del(s_buddy_label);
    }
    s_buddy_label = lv_label_create(parent);
    lv_obj_set_style_text_align(s_buddy_label, LV_TEXT_ALIGN_CENTER, 0);
    s_current_parent = parent;
    return s_buddy_label;
}

/**
 * 绘制 ASCII 文本到指定位置
 */
static void buddy_draw_ascii(lv_obj_t *canvas, lv_coord_t cx, lv_coord_t cy,
                             lv_color_t color, const char *text,
                             uint32_t tick, uint8_t state)
{
    lv_obj_t *label = buddy_ensure_label(canvas);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, LV_FONT_DEFAULT, 0);

    lv_coord_t off_y = buddy_anim_offset(tick, state);
    /* cx/cy 是角色中心在画布内的目标坐标；LV_ALIGN_CENTER 的偏移相对父容器中心，需换算 */
    lv_obj_align(label, LV_ALIGN_CENTER,
                 cx - lv_obj_get_width(canvas) / 2,
                 cy - lv_obj_get_height(canvas) / 2 + off_y);

    if (s_peek) {
        lv_obj_set_style_transform_zoom(label, 512, 0);
        lv_obj_set_style_transform_pivot_x(label, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(label, LV_PCT(50), 0);
    } else {
        lv_obj_set_style_transform_zoom(label, 256, 0);
    }
}

/* ==========================================================================
 * ASCII 艺术字库 [18 种角色][7 种状态]
 * 状态索引: 0=sleep, 1=idle, 2=busy, 3=attention, 4=celebrate, 5=dizzy, 6=heart
 * ========================================================================== */

static const char *const s_buddy_ascii[BUDDY_COUNT][7] = {
    /* 0: capybara (棕色) */
    {
        "   _____   \n"
        "  /     \\  \n"
        " (  -.-  ) \n"
        "  )  zZ (  \n"
        " /_______\\ ",
        "   _____   \n"
        "  /     \\  \n"
        " (  o.o  ) \n"
        "  )     (  \n"
        " /_______\\ ",
        "   _____   \n"
        "  /     \\  \n"
        " (  >.<  ) \n"
        "  ) ~~  (  \n"
        " /_______\\ ",
        "   _____   \n"
        "  /  !  \\  \n"
        " (  O.O  ) \n"
        "  )  !  (  \n"
        " /_______\\ ",
        "  \\  |  /  \n"
        "   _____   \n"
        "  ( ^.^ )  \n"
        "  )     (  \n"
        " /_______\\ ",
        "   _____   \n"
        "  /     \\  \n"
        " (  @.@  ) \n"
        "  )  ~  (  \n"
        " /_______\\ ",
        "   _____   \n"
        "  /     \\  \n"
        " (  v.v  ) \n"
        "  ) <3  (  \n"
        " /_______\\ "
    },
    /* 1: duck (黄色) */
    {
        "    __     \n"
        "  <(o )__  \n"
        "   (  ._)> \n"
        "    zZZ    ",
        "    __     \n"
        "  <(o )__  \n"
        "   (  ._)> \n"
        "    ^^^    ",
        "    __     \n"
        "  <(- )__  \n"
        "   ( >_)>  \n"
        "   ~~~~    ",
        "    __!    \n"
        "  <(O )__  \n"
        "   (  ._)> \n"
        "           ",
        "   \\ __ /  \n"
        "  <(^ )__  \n"
        "   (  ._)> \n"
        "           ",
        "    __     \n"
        "  <(@ )__  \n"
        "   ( ~_)>  \n"
        "           ",
        "    __     \n"
        "  <(v )__  \n"
        "   (w._)>  \n"
        "           "
    },
    /* 2: goose (白色) */
    {
        "      __   \n"
        "    <(o )__\n"
        "   ~(  ._)>\n"
        "     zZZ   ",
        "      __   \n"
        "    <(o )__\n"
        "     (  ._)>\n"
        "      ^^^  ",
        "      __   \n"
        "    <(- )__\n"
        "     ( >_)>\n"
        "     ~~~~  ",
        "      __   \n"
        "    <(O )__!\n"
        "     (  ._)>\n"
        "           ",
        "    \\    / \n"
        "      __   \n"
        "    <(^ )__\n"
        "     (  ._)>",
        "      __   \n"
        "    <(@ )__\n"
        "     ( ~_)>\n"
        "           ",
        "      __   \n"
        "    <(v )__\n"
        "     (w._)>\n"
        "           "
    },
    /* 3: blob (粉色) */
    {
        "   .---.   \n"
        "  /  -  \\  \n"
        " (   zZ   ) \n"
        "  \\_____/  ",
        "   .---.   \n"
        "  / o o \\  \n"
        " (   ▼    ) \n"
        "  \\_____/  ",
        "   .---.   \n"
        "  / > < \\  \n"
        " (  ~~~   ) \n"
        "  \\_____/  ",
        "   .---.   \n"
        "  / O O \\  \n"
        " (   !    ) \n"
        "  \\_____/  ",
        "    | |    \n"
        "   .---.   \n"
        "  ( ^ ^ )  \n"
        "   \\___/   ",
        "   .---.   \n"
        "  / @ @ \\  \n"
        " (  ~ ~   ) \n"
        "  \\_____/  ",
        "   .---.   \n"
        "  / v v \\  \n"
        " (   w    ) \n"
        "  \\_____/  "
    },
    /* 4: cat (橙色) */
    {
        "   /\\__/\\  \n"
        "  (  -  )  \n"
        "   ) zZ (  \n"
        "  (_____)  ",
        "   /\\__/\\  \n"
        "  ( o.o )  \n"
        "   )   (   \n"
        "  (_____)  ",
        "   /\\__/\\  \n"
        "  ( >.< )  \n"
        "   )~~(    \n"
        "  (_____)  ",
        "   /\\__/\\  \n"
        "  ( O.O )! \n"
        "   ) ! (   \n"
        "  (_____)  ",
        "    /\\     \n"
        "   /\\__/\\  \n"
        "  ( ^.^ )  \n"
        "   )   (   \n"
        "  (_____)  ",
        "   /\\__/\\  \n"
        "  ( @.@ )  \n"
        "   ) ~ (   \n"
        "  (_____)  ",
        "   /\\__/\\  \n"
        "  ( v.v )  \n"
        "   )<3 (   \n"
        "  (_____)  "
    },
    /* 5: dragon (绿色) */
    {
        "    /\\____/\\  \n"
        "   /  -  -  \\  \n"
        "  (    zZ    ) \n"
        "   \\________/  ",
        "    /\\____/\\  \n"
        "   /  o  o  \\  \n"
        "  (    ▼     ) \n"
        "   \\________/  ",
        "    /\\____/\\  \n"
        "   /  >  <  \\  \n"
        "  (   ~~~    ) \n"
        "   \\________/  ",
        "    /\\____/\\  \n"
        "   /  O  O  \\  \n"
        "  (    !     ) \n"
        "   \\________/  ",
        "      /\\      \n"
        "    /\\____/\\  \n"
        "   (  ^  ^  )  \n"
        "    \\______/   ",
        "    /\\____/\\  \n"
        "   /  @  @  \\  \n"
        "  (   ~ ~    ) \n"
        "   \\________/  ",
        "    /\\____/\\  \n"
        "   /  v  v  \\  \n"
        "  (    w     ) \n"
        "   \\________/  "
    },
    /* 6: octopus (紫色) */
    {
        "    .---.    \n"
        "   /  -  \\   \n"
        "  (   zZ   )  \n"
        "   ~| | |~   ",
        "    .---.    \n"
        "   / o o \\   \n"
        "  (   ▼    )  \n"
        "   ~| | |~   ",
        "    .---.    \n"
        "   / > < \\   \n"
        "  (  ~~~   )  \n"
        "   ~| | |~   ",
        "    .---.    \n"
        "   / O O \\   \n"
        "  (   !    )  \n"
        "   ~| | |~   ",
        "     | |     \n"
        "    .---.    \n"
        "   ( ^ ^ )   \n"
        "    ~| |~    ",
        "    .---.    \n"
        "   / @ @ \\   \n"
        "  (  ~ ~   )  \n"
        "   ~| | |~   ",
        "    .---.    \n"
        "   / v v \\   \n"
        "  (   w    )  \n"
        "   ~| | |~   "
    },
    /* 7: owl (棕色) */
    {
        "   ,___,   \n"
        "  [ o.o ]  \n"
        "   ) zZ (  \n"
        "  -\"---\"-  ",
        "   ,___,   \n"
        "  [ o.o ]  \n"
        "   )   (   \n"
        "  -\"---\"-  ",
        "   ,___,   \n"
        "  [ >.< ]  \n"
        "   )~~(    \n"
        "  -\"---\"-  ",
        "   ,___,   \n"
        "  [ O.O ]! \n"
        "   ) ! (   \n"
        "  -\"---\"-  ",
        "    \\ /    \n"
        "   ,___,   \n"
        "  [ ^.^ ]  \n"
        "   )   (   \n"
        "  -\"---\"-  ",
        "   ,___,   \n"
        "  [ @.@ ]  \n"
        "   ) ~ (   \n"
        "  -\"---\"-  ",
        "   ,___,   \n"
        "  [ v.v ]  \n"
        "   )<3 (   \n"
        "  -\"---\"-  "
    },
    /* 8: penguin (黑白色) */
    {
        "   .---.   \n"
        "  /  -  \\  \n"
        " (   zZ   ) \n"
        "  \\_______/ ",
        "   .---.   \n"
        "  / o o \\  \n"
        " (   ▼    ) \n"
        "  \\_______/ ",
        "   .---.   \n"
        "  / > < \\  \n"
        " (  ~~~   ) \n"
        "  \\_______/ ",
        "   .---.   \n"
        "  / O O \\  \n"
        " (   !    ) \n"
        "  \\_______/ ",
        "    | |    \n"
        "   .---.   \n"
        "  ( ^ ^ )  \n"
        "   \\_____/  ",
        "   .---.   \n"
        "  / @ @ \\  \n"
        " (  ~ ~   ) \n"
        "  \\_______/ ",
        "   .---.   \n"
        "  / v v \\  \n"
        " (   w    ) \n"
        "  \\_______/ "
    },
    /* 9: turtle (绿色) */
    {
        "    ___    \n"
        "   /   \\   \n"
        "  |  -  |  \n"
        "   \\_zZ_/  ",
        "    ___    \n"
        "   /   \\   \n"
        "  | o o |  \n"
        "   \\_ _/   ",
        "    ___    \n"
        "   /   \\   \n"
        "  | > < |  \n"
        "   \\_~_/   ",
        "    ___    \n"
        "   / ! \\   \n"
        "  | O O |  \n"
        "   \\_ _/   ",
        "     /\\    \n"
        "    /  \\   \n"
        "   | ^^ |  \n"
        "    \\__/   ",
        "    ___    \n"
        "   /   \\   \n"
        "  | @ @ |  \n"
        "   \\_~_/   ",
        "    ___    \n"
        "   /   \\   \n"
        "  |v   v|  \n"
        "   \\_w_/   "
    },
    /* 10: snail (棕色) */
    {
        "   @    zZ \n"
        "  /|\\     \n"
        "  | |      \n"
        "  @@@      ",
        "   @       \n"
        "  /|\\     \n"
        "  | |      \n"
        "  @@@      ",
        "   @       \n"
        "  /|\\     \n"
        "  |~|      \n"
        "  @@@      ",
        "   @  !    \n"
        "  /|\\     \n"
        "  | |      \n"
        "  @@@      ",
        "  \\ @ /   \n"
        "  /|\\     \n"
        "  | |      \n"
        "  @@@      ",
        "   @       \n"
        "  /|\\     \n"
        "  |~|      \n"
        "  @@@      ",
        "   @       \n"
        "  /|\\     \n"
        "  | |      \n"
        "  @v@      "
    },
    /* 11: ghost (灰白色) */
    {
        "   .---.   \n"
        "  /  -  \\  \n"
        " (   zZ   ) \n"
        "  |     |  \n"
        "  ~     ~  ",
        "   .---.   \n"
        "  / o o \\  \n"
        " (   ▼    ) \n"
        "  |     |  \n"
        "  ~     ~  ",
        "   .---.   \n"
        "  / > < \\  \n"
        " (  ~~~   ) \n"
        "  |     |  \n"
        "  ~     ~  ",
        "   .---.   \n"
        "  / O O \\  \n"
        " (   !    ) \n"
        "  |     |  \n"
        "  ~     ~  ",
        "    | |    \n"
        "   .---.   \n"
        "  ( ^ ^ )  \n"
        "   |   |   \n"
        "   ~   ~   ",
        "   .---.   \n"
        "  / @ @ \\  \n"
        " (  ~ ~   ) \n"
        "  |     |  \n"
        "  ~     ~  ",
        "   .---.   \n"
        "  / v v \\  \n"
        " (   w    ) \n"
        "  |     |  \n"
        "  ~     ~  "
    },
    /* 12: axolotl (粉色) */
    {
        "    .---.    \n"
        "   /  -  \\   \n"
        "  (   zZ   )  \n"
        "   ~<{ }>~   ",
        "    .---.    \n"
        "   / o o \\   \n"
        "  (   ▼    )  \n"
        "   ~<{ }>~   ",
        "    .---.    \n"
        "   / > < \\   \n"
        "  (  ~~~   )  \n"
        "   ~<{ }>~   ",
        "    .---.    \n"
        "   / O O \\   \n"
        "  (   !    )  \n"
        "   ~<{ }>~   ",
        "     | |     \n"
        "    .---.    \n"
        "   ( ^ ^ )   \n"
        "    ~< >~    ",
        "    .---.    \n"
        "   / @ @ \\   \n"
        "  (  ~ ~   )  \n"
        "   ~<{ }>~   ",
        "    .---.    \n"
        "   / v v \\   \n"
        "  (   w    )  \n"
        "   ~<{ }>~   "
    },
    /* 13: cactus (绿色) */
    {
        "    | |    \n"
        "   .---.   \n"
        "  |  -  |  \n"
        "   \\_zZ_/  ",
        "    | |    \n"
        "   .---.   \n"
        "  | o o |  \n"
        "   \\_ _/   ",
        "    | |    \n"
        "   .---.   \n"
        "  | > < |  \n"
        "   \\_~_/   ",
        "    |!|    \n"
        "   .---.   \n"
        "  | O O |  \n"
        "   \\_ _/   ",
        "   \\ | /   \n"
        "    .-.    \n"
        "   ( ^ )   \n"
        "    \\_/    ",
        "    | |    \n"
        "   .---.   \n"
        "  | @ @ |  \n"
        "   \\_~_/   ",
        "    | |    \n"
        "   .---.   \n"
        "  |v   v|  \n"
        "   \\_w_/   "
    },
    /* 14: robot (灰蓝色) */
    {
        "   [___]   \n"
        "  |  -  |  \n"
        "  |  zZ |  \n"
        "   [___]   ",
        "   [___]   \n"
        "  | o o |  \n"
        "  |  ▼  |  \n"
        "   [___]   ",
        "   [___]   \n"
        "  | > < |  \n"
        "  | ~~~ |  \n"
        "   [___]   ",
        "   [___]   \n"
        "  | O O |  \n"
        "  |  !  |  \n"
        "   [___]   ",
        "    \\ /    \n"
        "   [___]   \n"
        "  | ^ ^ |  \n"
        "   [___]   ",
        "   [___]   \n"
        "  | @ @ |  \n"
        "  | ~ ~ |  \n"
        "   [___]   ",
        "   [___]   \n"
        "  |v   v|  \n"
        "  |  w  |  \n"
        "   [___]   "
    },
    /* 15: rabbit (灰白色) */
    {
        "   /\\ /\\   \n"
        "  (  -  )  \n"
        "   ) zZ (  \n"
        "  (_____)  ",
        "   /\\ /\\   \n"
        "  ( o.o )  \n"
        "   )   (   \n"
        "  (_____)  ",
        "   /\\ /\\   \n"
        "  ( >.< )  \n"
        "   )~~(    \n"
        "  (_____)  ",
        "   /\\ /\\   \n"
        "  ( O.O )! \n"
        "   ) ! (   \n"
        "  (_____)  ",
        "    / \\    \n"
        "   /\\ /\\   \n"
        "  ( ^.^ )  \n"
        "   )   (   \n"
        "  (_____)  ",
        "   /\\ /\\   \n"
        "  ( @.@ )  \n"
        "   ) ~ (   \n"
        "  (_____)  ",
        "   /\\ /\\   \n"
        "  ( v.v )  \n"
        "   )<3 (   \n"
        "  (_____)  "
    },
    /* 16: mushroom (红色) */
    {
        "    ___    \n"
        "   /   \\   \n"
        "  |  -  |  \n"
        "   \\_zZ/   \n"
        "   |   |   ",
        "    ___    \n"
        "   /   \\   \n"
        "  | o o |  \n"
        "   \\___/   \n"
        "   |   |   ",
        "    ___    \n"
        "   /   \\   \n"
        "  | > < |  \n"
        "   \\_~_/   \n"
        "   |   |   ",
        "    ___    \n"
        "   / ! \\   \n"
        "  | O O |  \n"
        "   \\_ _/   \n"
        "   |   |   ",
        "   \\   /   \n"
        "    ___    \n"
        "   | ^ |   \n"
        "   \\___/   \n"
        "   |   |   ",
        "    ___    \n"
        "   /   \\   \n"
        "  | @ @ |  \n"
        "   \\_~_/   \n"
        "   |   |   ",
        "    ___    \n"
        "   /   \\   \n"
        "  |v   v|  \n"
        "   \\_w_/   \n"
        "   |   |   "
    },
    /* 17: chonk (灰色) */
    {
        "  .-------. \n"
        " (   - -   )\n"
        "  )  zZ   ( \n"
        " (_________)",
        "  .-------. \n"
        " (   o o   )\n"
        "  )       ( \n"
        " (_________)",
        "  .-------. \n"
        " (   > <   )\n"
        "  )  ~~~  ( \n"
        " (_________)",
        "  .-------. \n"
        " (   O O   )!\n"
        "  )   !   ( \n"
        " (_________)",
        "   \\  |  /  \n"
        "  .-------. \n"
        " (   ^ ^   )\n"
        "  )       ( \n"
        " (_________)",
        "  .-------. \n"
        " (   @ @   )\n"
        "  )  ~ ~  ( \n"
        " (_________)",
        "  .-------. \n"
        " (   v.v   )\n"
        "  )  <3  ( \n"
        " (_________)"
    }
};

/* ==========================================================================
 * 状态绘制函数生成宏
 * ========================================================================== */

#define BUDDY_STATE_FN(species, state_suffix, species_idx, state_idx)           \
    static void species##state_suffix(uint32_t tick, lv_obj_t *canvas,           \
                                      lv_coord_t cx, lv_coord_t cy,               \
                                      lv_color_t body_color)                      \
    {                                                                             \
        buddy_draw_ascii(canvas, cx, cy, body_color,                              \
                         s_buddy_ascii[species_idx][state_idx], tick, state_idx); \
    }

/* 为每个物种生成 7 个状态函数 */
BUDDY_STATE_FN(capybara, _sleep, 0, 0)
BUDDY_STATE_FN(capybara, _idle, 0, 1)
BUDDY_STATE_FN(capybara, _busy, 0, 2)
BUDDY_STATE_FN(capybara, _attention, 0, 3)
BUDDY_STATE_FN(capybara, _celebrate, 0, 4)
BUDDY_STATE_FN(capybara, _dizzy, 0, 5)
BUDDY_STATE_FN(capybara, _heart, 0, 6)

BUDDY_STATE_FN(duck, _sleep, 1, 0)
BUDDY_STATE_FN(duck, _idle, 1, 1)
BUDDY_STATE_FN(duck, _busy, 1, 2)
BUDDY_STATE_FN(duck, _attention, 1, 3)
BUDDY_STATE_FN(duck, _celebrate, 1, 4)
BUDDY_STATE_FN(duck, _dizzy, 1, 5)
BUDDY_STATE_FN(duck, _heart, 1, 6)

BUDDY_STATE_FN(goose, _sleep, 2, 0)
BUDDY_STATE_FN(goose, _idle, 2, 1)
BUDDY_STATE_FN(goose, _busy, 2, 2)
BUDDY_STATE_FN(goose, _attention, 2, 3)
BUDDY_STATE_FN(goose, _celebrate, 2, 4)
BUDDY_STATE_FN(goose, _dizzy, 2, 5)
BUDDY_STATE_FN(goose, _heart, 2, 6)

BUDDY_STATE_FN(blob, _sleep, 3, 0)
BUDDY_STATE_FN(blob, _idle, 3, 1)
BUDDY_STATE_FN(blob, _busy, 3, 2)
BUDDY_STATE_FN(blob, _attention, 3, 3)
BUDDY_STATE_FN(blob, _celebrate, 3, 4)
BUDDY_STATE_FN(blob, _dizzy, 3, 5)
BUDDY_STATE_FN(blob, _heart, 3, 6)

BUDDY_STATE_FN(cat, _sleep, 4, 0)
BUDDY_STATE_FN(cat, _idle, 4, 1)
BUDDY_STATE_FN(cat, _busy, 4, 2)
BUDDY_STATE_FN(cat, _attention, 4, 3)
BUDDY_STATE_FN(cat, _celebrate, 4, 4)
BUDDY_STATE_FN(cat, _dizzy, 4, 5)
BUDDY_STATE_FN(cat, _heart, 4, 6)

BUDDY_STATE_FN(dragon, _sleep, 5, 0)
BUDDY_STATE_FN(dragon, _idle, 5, 1)
BUDDY_STATE_FN(dragon, _busy, 5, 2)
BUDDY_STATE_FN(dragon, _attention, 5, 3)
BUDDY_STATE_FN(dragon, _celebrate, 5, 4)
BUDDY_STATE_FN(dragon, _dizzy, 5, 5)
BUDDY_STATE_FN(dragon, _heart, 5, 6)

BUDDY_STATE_FN(octopus, _sleep, 6, 0)
BUDDY_STATE_FN(octopus, _idle, 6, 1)
BUDDY_STATE_FN(octopus, _busy, 6, 2)
BUDDY_STATE_FN(octopus, _attention, 6, 3)
BUDDY_STATE_FN(octopus, _celebrate, 6, 4)
BUDDY_STATE_FN(octopus, _dizzy, 6, 5)
BUDDY_STATE_FN(octopus, _heart, 6, 6)

BUDDY_STATE_FN(owl, _sleep, 7, 0)
BUDDY_STATE_FN(owl, _idle, 7, 1)
BUDDY_STATE_FN(owl, _busy, 7, 2)
BUDDY_STATE_FN(owl, _attention, 7, 3)
BUDDY_STATE_FN(owl, _celebrate, 7, 4)
BUDDY_STATE_FN(owl, _dizzy, 7, 5)
BUDDY_STATE_FN(owl, _heart, 7, 6)

BUDDY_STATE_FN(penguin, _sleep, 8, 0)
BUDDY_STATE_FN(penguin, _idle, 8, 1)
BUDDY_STATE_FN(penguin, _busy, 8, 2)
BUDDY_STATE_FN(penguin, _attention, 8, 3)
BUDDY_STATE_FN(penguin, _celebrate, 8, 4)
BUDDY_STATE_FN(penguin, _dizzy, 8, 5)
BUDDY_STATE_FN(penguin, _heart, 8, 6)

BUDDY_STATE_FN(turtle, _sleep, 9, 0)
BUDDY_STATE_FN(turtle, _idle, 9, 1)
BUDDY_STATE_FN(turtle, _busy, 9, 2)
BUDDY_STATE_FN(turtle, _attention, 9, 3)
BUDDY_STATE_FN(turtle, _celebrate, 9, 4)
BUDDY_STATE_FN(turtle, _dizzy, 9, 5)
BUDDY_STATE_FN(turtle, _heart, 9, 6)

BUDDY_STATE_FN(snail, _sleep, 10, 0)
BUDDY_STATE_FN(snail, _idle, 10, 1)
BUDDY_STATE_FN(snail, _busy, 10, 2)
BUDDY_STATE_FN(snail, _attention, 10, 3)
BUDDY_STATE_FN(snail, _celebrate, 10, 4)
BUDDY_STATE_FN(snail, _dizzy, 10, 5)
BUDDY_STATE_FN(snail, _heart, 10, 6)

BUDDY_STATE_FN(ghost, _sleep, 11, 0)
BUDDY_STATE_FN(ghost, _idle, 11, 1)
BUDDY_STATE_FN(ghost, _busy, 11, 2)
BUDDY_STATE_FN(ghost, _attention, 11, 3)
BUDDY_STATE_FN(ghost, _celebrate, 11, 4)
BUDDY_STATE_FN(ghost, _dizzy, 11, 5)
BUDDY_STATE_FN(ghost, _heart, 11, 6)

BUDDY_STATE_FN(axolotl, _sleep, 12, 0)
BUDDY_STATE_FN(axolotl, _idle, 12, 1)
BUDDY_STATE_FN(axolotl, _busy, 12, 2)
BUDDY_STATE_FN(axolotl, _attention, 12, 3)
BUDDY_STATE_FN(axolotl, _celebrate, 12, 4)
BUDDY_STATE_FN(axolotl, _dizzy, 12, 5)
BUDDY_STATE_FN(axolotl, _heart, 12, 6)

BUDDY_STATE_FN(cactus, _sleep, 13, 0)
BUDDY_STATE_FN(cactus, _idle, 13, 1)
BUDDY_STATE_FN(cactus, _busy, 13, 2)
BUDDY_STATE_FN(cactus, _attention, 13, 3)
BUDDY_STATE_FN(cactus, _celebrate, 13, 4)
BUDDY_STATE_FN(cactus, _dizzy, 13, 5)
BUDDY_STATE_FN(cactus, _heart, 13, 6)

BUDDY_STATE_FN(robot, _sleep, 14, 0)
BUDDY_STATE_FN(robot, _idle, 14, 1)
BUDDY_STATE_FN(robot, _busy, 14, 2)
BUDDY_STATE_FN(robot, _attention, 14, 3)
BUDDY_STATE_FN(robot, _celebrate, 14, 4)
BUDDY_STATE_FN(robot, _dizzy, 14, 5)
BUDDY_STATE_FN(robot, _heart, 14, 6)

BUDDY_STATE_FN(rabbit, _sleep, 15, 0)
BUDDY_STATE_FN(rabbit, _idle, 15, 1)
BUDDY_STATE_FN(rabbit, _busy, 15, 2)
BUDDY_STATE_FN(rabbit, _attention, 15, 3)
BUDDY_STATE_FN(rabbit, _celebrate, 15, 4)
BUDDY_STATE_FN(rabbit, _dizzy, 15, 5)
BUDDY_STATE_FN(rabbit, _heart, 15, 6)

BUDDY_STATE_FN(mushroom, _sleep, 16, 0)
BUDDY_STATE_FN(mushroom, _idle, 16, 1)
BUDDY_STATE_FN(mushroom, _busy, 16, 2)
BUDDY_STATE_FN(mushroom, _attention, 16, 3)
BUDDY_STATE_FN(mushroom, _celebrate, 16, 4)
BUDDY_STATE_FN(mushroom, _dizzy, 16, 5)
BUDDY_STATE_FN(mushroom, _heart, 16, 6)

BUDDY_STATE_FN(chonk, _sleep, 17, 0)
BUDDY_STATE_FN(chonk, _idle, 17, 1)
BUDDY_STATE_FN(chonk, _busy, 17, 2)
BUDDY_STATE_FN(chonk, _attention, 17, 3)
BUDDY_STATE_FN(chonk, _celebrate, 17, 4)
BUDDY_STATE_FN(chonk, _dizzy, 17, 5)
BUDDY_STATE_FN(chonk, _heart, 17, 6)

/* ==========================================================================
 * BuddySpecies 数组定义
 * ========================================================================== */

#define BUDDY_STATES(name)                                                      \
    {                                                                           \
        name##_sleep, name##_idle, name##_busy, name##_attention,               \
        name##_celebrate, name##_dizzy, name##_heart                            \
    }

static BuddySpecies s_species[BUDDY_COUNT];

static void buddy_init_species(int idx, const char *name, lv_color_t color,
                               BuddyStateFn sleep, BuddyStateFn idle,
                               BuddyStateFn busy, BuddyStateFn attention,
                               BuddyStateFn celebrate, BuddyStateFn dizzy,
                               BuddyStateFn heart)
{
    s_species[idx].name = name;
    s_species[idx].body_color = color;
    s_species[idx].states[0] = sleep;
    s_species[idx].states[1] = idle;
    s_species[idx].states[2] = busy;
    s_species[idx].states[3] = attention;
    s_species[idx].states[4] = celebrate;
    s_species[idx].states[5] = dizzy;
    s_species[idx].states[6] = heart;
}

/* ==========================================================================
 * 公共 API 实现
 * ========================================================================== */

void buddy_anim_init(void)
{
    s_current_species = 0;
    s_peek = false;
    g_buddy_tick = 0;
    s_buddy_label = NULL;
    s_current_parent = NULL;
    s_cx = 0;
    s_cy = 0;

    /* Runtime init — lv_color_hex() is not a compile-time constant */
    buddy_init_species(0,  "capybara", lv_color_hex(0x8B6914), capybara_sleep, capybara_idle, capybara_busy, capybara_attention, capybara_celebrate, capybara_dizzy, capybara_heart);
    buddy_init_species(1,  "duck",     lv_color_hex(0xFFD700), duck_sleep,     duck_idle,     duck_busy,     duck_attention,     duck_celebrate,     duck_dizzy,     duck_heart);
    buddy_init_species(2,  "goose",    lv_color_hex(0xF5F5F5), goose_sleep,    goose_idle,    goose_busy,    goose_attention,    goose_celebrate,    goose_dizzy,    goose_heart);
    buddy_init_species(3,  "blob",     lv_color_hex(0xFF69B4), blob_sleep,     blob_idle,     blob_busy,     blob_attention,     blob_celebrate,     blob_dizzy,     blob_heart);
    buddy_init_species(4,  "cat",      lv_color_hex(0xFFA500), cat_sleep,      cat_idle,      cat_busy,      cat_attention,      cat_celebrate,      cat_dizzy,      cat_heart);
    buddy_init_species(5,  "dragon",   lv_color_hex(0x00C864), dragon_sleep,   dragon_idle,   dragon_busy,   dragon_attention,   dragon_celebrate,   dragon_dizzy,   dragon_heart);
    buddy_init_species(6,  "octopus",  lv_color_hex(0x9370DB), octopus_sleep,  octopus_idle,  octopus_busy,  octopus_attention,  octopus_celebrate,  octopus_dizzy,  octopus_heart);
    buddy_init_species(7,  "owl",      lv_color_hex(0x8B4513), owl_sleep,      owl_idle,      owl_busy,      owl_attention,      owl_celebrate,      owl_dizzy,      owl_heart);
    buddy_init_species(8,  "penguin",  lv_color_hex(0x333333), penguin_sleep,  penguin_idle,  penguin_busy,  penguin_attention,  penguin_celebrate,  penguin_dizzy,  penguin_heart);
    buddy_init_species(9,  "turtle",   lv_color_hex(0x228B22), turtle_sleep,   turtle_idle,   turtle_busy,   turtle_attention,   turtle_celebrate,   turtle_dizzy,   turtle_heart);
    buddy_init_species(10, "snail",    lv_color_hex(0xA0522D), snail_sleep,    snail_idle,    snail_busy,    snail_attention,    snail_celebrate,    snail_dizzy,    snail_heart);
    buddy_init_species(11, "ghost",    lv_color_hex(0xE0E0E0), ghost_sleep,    ghost_idle,    ghost_busy,    ghost_attention,    ghost_celebrate,    ghost_dizzy,    ghost_heart);
    buddy_init_species(12, "axolotl",  lv_color_hex(0xFFB6C1), axolotl_sleep,  axolotl_idle,  axolotl_busy,  axolotl_attention,  axolotl_celebrate,  axolotl_dizzy,  axolotl_heart);
    buddy_init_species(13, "cactus",   lv_color_hex(0x32CD32), cactus_sleep,   cactus_idle,   cactus_busy,   cactus_attention,   cactus_celebrate,   cactus_dizzy,   cactus_heart);
    buddy_init_species(14, "robot",    lv_color_hex(0x708090), robot_sleep,    robot_idle,    robot_busy,    robot_attention,    robot_celebrate,    robot_dizzy,    robot_heart);
    buddy_init_species(15, "rabbit",   lv_color_hex(0xD3D3D3), rabbit_sleep,   rabbit_idle,   rabbit_busy,   rabbit_attention,   rabbit_celebrate,   rabbit_dizzy,   rabbit_heart);
    buddy_init_species(16, "mushroom", lv_color_hex(0xFF4444), mushroom_sleep, mushroom_idle, mushroom_busy, mushroom_attention, mushroom_celebrate, mushroom_dizzy, mushroom_heart);
    buddy_init_species(17, "chonk",    lv_color_hex(0x808080), chonk_sleep,    chonk_idle,    chonk_busy,    chonk_attention,    chonk_celebrate,    chonk_dizzy,    chonk_heart);
}

void buddy_anim_tick(uint8_t persona_state, uint32_t global_tick)
{
    g_buddy_tick = global_tick;
    if (s_current_parent && s_current_species < BUDDY_COUNT) {
        uint8_t state = persona_state % 7;
        BuddySpecies *sp = &s_species[s_current_species];
        sp->states[state](global_tick, s_current_parent, s_cx, s_cy, sp->body_color);
    }
}

void buddy_anim_invalidate(void)
{
    if (s_buddy_label) {
        lv_obj_del(s_buddy_label);
        s_buddy_label = NULL;
        s_current_parent = NULL;
    }
}

void buddy_anim_set_species_by_name(const char *name)
{
    if (!name) return;
    for (int i = 0; i < BUDDY_COUNT; i++) {
        if (strcmp(name, s_species[i].name) == 0) {
            s_current_species = (uint8_t)i;
            return;
        }
    }
}

void buddy_anim_set_species_idx(uint8_t idx)
{
    if (idx < BUDDY_COUNT) {
        s_current_species = idx;
    }
}

void buddy_anim_next_species(void)
{
    s_current_species = (s_current_species + 1) % BUDDY_COUNT;
}

void buddy_anim_prev_species(void)
{
    s_current_species = (s_current_species + BUDDY_COUNT - 1) % BUDDY_COUNT;
}

uint8_t buddy_anim_get_species_idx(void)
{
    return s_current_species;
}

uint8_t buddy_anim_get_species_count(void)
{
    return BUDDY_COUNT;
}

const char *buddy_anim_get_species_name(void)
{
    return s_species[s_current_species].name;
}

void buddy_anim_set_peek(bool peek)
{
    s_peek = peek;
}

bool buddy_anim_is_peek(void)
{
    return s_peek;
}

void buddy_anim_render_to(lv_obj_t *parent, lv_coord_t cx, lv_coord_t cy)
{
    if (s_buddy_label && s_current_parent != parent) {
        lv_obj_del(s_buddy_label);
        s_buddy_label = NULL;
    }
    s_current_parent = parent;
    s_cx = cx;
    s_cy = cy;
}
