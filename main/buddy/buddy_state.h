/**
 * @file buddy_state.h
 * Buddy 核心状态机 - 从 claude-desktop-buddy 迁移
 * 管理 7 种 Persona 状态、Claude 会话数据、审批提示
 */

#ifndef BUDDY_STATE_H
#define BUDDY_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 7 种 Persona 状态 */
typedef enum {
    PERSONA_SLEEP = 0,
    PERSONA_IDLE,
    PERSONA_BUSY,
    PERSONA_ATTENTION,
    PERSONA_CELEBRATE,
    PERSONA_DIZZY,
    PERSONA_HEART,
    PERSONA_COUNT
} PersonaState;

/* Claude 会话状态 */
typedef struct {
    uint8_t  sessions_total;
    uint8_t  sessions_running;
    uint8_t  sessions_waiting;
    bool     recently_completed;
    uint32_t tokens_today;
    uint32_t last_updated_ms;
    char     msg[24];
    bool     connected;
    char     lines[8][92];
    uint8_t  n_lines;
    uint16_t line_gen;          /* 行变化时递增，用于 UI 重置滚动 */
    char     prompt_id[40];     /* 待审批请求 ID；空 = 无审批 */
    char     prompt_tool[20];
    char     prompt_hint[44];
} ClaudeState;

/* 全局状态 */
typedef struct {
    PersonaState base_state;      /* 从会话派生的基础状态 */
    PersonaState active_state;    /* 当前激活状态（含 one-shot） */
    uint32_t     oneshot_until_ms;
    uint32_t     last_interact_ms;
    bool         screen_off;
    bool         napping;
    uint32_t     nap_start_ms;
    uint32_t     prompt_arrived_ms;
    bool         response_sent;
    char         last_prompt_id[40];
    uint16_t     last_line_gen;
    uint8_t      msg_scroll;
    uint32_t     wake_transition_until_ms;
    uint32_t     playful_until_ms;
    bool         demo_mode;
    uint8_t      demo_idx;
    uint32_t     demo_next_ms;
    bool         rtc_valid;
    uint32_t     last_live_ms;
    bool         swallow_btn_a;
    bool         swallow_btn_b;
} BuddyRuntime;

/* 显示模式 */
typedef enum {
    DISP_NORMAL = 0,
    DISP_PET,
    DISP_INFO,
    DISP_COUNT
} DisplayMode;

/* 页面索引 */
typedef struct {
    DisplayMode display_mode;
    uint8_t     info_page;
    uint8_t     pet_page;
    uint8_t     menu_sel;
    bool        menu_open;
    bool        settings_open;
    uint8_t     settings_sel;
    bool        reset_open;
    uint8_t     reset_sel;
    uint32_t    reset_confirm_until;
    uint8_t     reset_confirm_idx;
    bool        btn_a_long;
    uint8_t     bright_level;     /* 0..4 */
    bool        buddy_mode;       /* true=ASCII, false=GIF */
    bool        gif_available;
} BuddyUIState;

/* 初始化 */
void buddy_state_init(void);

/* 状态机 */
PersonaState buddy_derive_state(const ClaudeState *s);
void         buddy_trigger_oneshot(PersonaState s, uint32_t dur_ms);
void         buddy_update_state(void);

/* 数据轮询（从 UART 解析 JSON） */
void buddy_data_poll(ClaudeState *out);
void buddy_apply_json_line(const char *line, ClaudeState *out);

/* 连接状态 */
bool buddy_data_connected(void);
const char* buddy_scenario_name(void);

/* Demo 模式 */
void buddy_set_demo(bool on);
bool buddy_is_demo(void);

/* 运行时访问 */
ClaudeState*   buddy_get_claude_state(void);
BuddyRuntime*  buddy_get_runtime(void);
BuddyUIState*  buddy_get_ui_state(void);
const char*    buddy_state_name(PersonaState s);

/* 时间同步 */
bool buddy_rtc_valid(void);
void buddy_set_rtc_valid(bool v);

/* 提示处理 */
bool buddy_has_pending_prompt(void);
void buddy_clear_prompt(void);

/* 发送命令到上位机 */
void buddy_send_cmd(const char *json);

/* 外部数据输入 */
void buddy_feed_usb_line(const char *data, size_t len);

/* 本地时间查询 */
bool buddy_get_local_time(int *hour, int *min, int *sec,
                          int *year, int *month, int *day, int *dow);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_STATE_H */
