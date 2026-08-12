/**
 * @file buddy_state.c
 * Buddy 核心状态机实现
 */

#include "buddy_state.h"
#include "buddy_stats.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

static const char *TAG = "BUDDY_STATE";

/* 全局状态实例 */
static ClaudeState  s_claude = {0};
static BuddyRuntime s_runtime = {
    .swallow_btn_a = false,
    .swallow_btn_b = false,
};

static BuddyUIState s_ui = {
    .display_mode = DISP_NORMAL,
    .info_page = 0,
    .pet_page = 0,
    .menu_sel = 0,
    .menu_open = false,
    .settings_open = false,
    .settings_sel = 0,
    .reset_open = false,
    .reset_sel = 0,
    .reset_confirm_until = 0,
    .reset_confirm_idx = 0xFF,
    .btn_a_long = false,
    .bright_level = 4,
    .buddy_mode = false,
    .gif_available = false,
};

static const char* STATE_NAMES[] = {
    "sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart"
};

/* 接收行缓冲 */
#define LINE_BUF_SIZE 1024
typedef struct {
    char buf[LINE_BUF_SIZE];
    uint16_t len;
} LineBuf;

static LineBuf s_usb_line = {0};

/* 外部声明（由通信模块实现） */
extern void uart_send_raw(const char *data, size_t len);

/* 模拟场景（用于 demo 模式） */
typedef struct {
    const char *name;
    uint8_t total, running, waiting;
    bool completed;
    uint32_t tokens;
} FakeScenario;

static const FakeScenario FAKES[] = {
    {"asleep", 0, 0, 0, false, 0},
    {"one idle", 1, 0, 0, false, 12000},
    {"busy", 4, 3, 0, false, 89000},
    {"attention", 2, 1, 1, false, 45000},
    {"completed", 1, 0, 0, true, 142000},
};
#define FAKE_COUNT (sizeof(FAKES) / sizeof(FAKES[0]))

/* 时间同步：软件 RTC */
static struct {
    uint32_t epoch_sec;
    int32_t  tz_offset_sec;
    uint32_t set_at_ms;
} s_rtc = {0};

void buddy_state_init(void) {
    memset(&s_claude, 0, sizeof(s_claude));
    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime.base_state = PERSONA_SLEEP;
    s_runtime.active_state = PERSONA_SLEEP;
    ESP_LOGI(TAG, "Buddy state initialized");
}

const char* buddy_state_name(PersonaState s) {
    if (s < PERSONA_COUNT) return STATE_NAMES[s];
    return "unknown";
}

ClaudeState* buddy_get_claude_state(void) { return &s_claude; }
BuddyRuntime* buddy_get_runtime(void) { return &s_runtime; }
BuddyUIState* buddy_get_ui_state(void) { return &s_ui; }

/* 状态推导：从 Claude 会话状态推导基础 Persona 状态 */
PersonaState buddy_derive_state(const ClaudeState *s) {
    if (!s->connected)           return PERSONA_IDLE;
    if (s->sessions_waiting > 0)  return PERSONA_ATTENTION;
    if (s->recently_completed)    return PERSONA_CELEBRATE;
    if (s->sessions_running >= 3) return PERSONA_BUSY;
    return PERSONA_IDLE;
}

void buddy_trigger_oneshot(PersonaState s, uint32_t dur_ms) {
    s_runtime.active_state = s;
    s_runtime.oneshot_until_ms = esp_timer_get_time() / 1000 + dur_ms;
}

void buddy_update_state(void) {
    uint32_t now_ms = esp_timer_get_time() / 1000;

    /* 唤醒过渡：唤醒后 12s 内保持非 sleep */
    if (s_runtime.base_state == PERSONA_IDLE &&
        (int32_t)(now_ms - s_runtime.wake_transition_until_ms) < 0) {
        /* 保持 idle，不强制 sleep */
    }

    /* one-shot 超时后恢复基础状态 */
    if ((int32_t)(now_ms - s_runtime.oneshot_until_ms) >= 0) {
        s_runtime.active_state = s_runtime.base_state;
    }
}

/* ========== JSON 解析 ========== */

#include "cJSON.h"

/* 将非 ASCII 字节替换为随机 Matrix 风格符号 */
static void matrixify(char *s) {
    static const char POOL[] = "01<>{}[]/\\|*~$%#@&=+-_:;.!?ABCDEFGHJKLMNPQRSTVWXYZ";
    static const int POOL_N = sizeof(POOL) - 1;
    for (; *s; s++) {
        if ((uint8_t)*s > 127) {
            *s = POOL[esp_random() % POOL_N];
        }
    }
}

/* 处理角色包传输命令（外部声明） */
extern bool xfer_command(cJSON *doc);
extern void xfer_init(void);

static void apply_json(const char *line, ClaudeState *out) {
    cJSON *doc = cJSON_Parse(line);
    if (!doc) return;

    /* 角色包传输命令 */
    if (xfer_command(doc)) {
        s_runtime.last_live_ms = esp_timer_get_time() / 1000;
        cJSON_Delete(doc);
        return;
    }

    /* 时间同步: {"time":[epoch_sec, tz_offset_sec]} */
    cJSON *time_arr = cJSON_GetObjectItem(doc, "time");
    if (cJSON_IsArray(time_arr) && cJSON_GetArraySize(time_arr) == 2) {
        s_rtc.epoch_sec = (uint32_t)cJSON_GetArrayItem(time_arr, 0)->valuedouble;
        s_rtc.tz_offset_sec = (int32_t)cJSON_GetArrayItem(time_arr, 1)->valuedouble;
        s_rtc.set_at_ms = esp_timer_get_time() / 1000;
        s_runtime.rtc_valid = true;
        s_runtime.last_live_ms = s_rtc.set_at_ms;
        cJSON_Delete(doc);
        return;
    }

    /* 会话状态 */
    cJSON *item;
    if ((item = cJSON_GetObjectItem(doc, "total")))     out->sessions_total = (uint8_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(doc, "running")))   out->sessions_running = (uint8_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(doc, "waiting")))   out->sessions_waiting = (uint8_t)item->valuedouble;
    if ((item = cJSON_GetObjectItem(doc, "completed"))) out->recently_completed = cJSON_IsTrue(item);

    /* Tokens */
    if ((item = cJSON_GetObjectItem(doc, "tokens"))) {
        uint32_t bridge_tokens = (uint32_t)item->valuedouble;
        buddy_stats_on_bridge_tokens(bridge_tokens);
    }
    if ((item = cJSON_GetObjectItem(doc, "tokens_today"))) {
        out->tokens_today = (uint32_t)item->valuedouble;
    }

    /* 消息 */
    if ((item = cJSON_GetObjectItem(doc, "msg")) && cJSON_IsString(item)) {
        strncpy(out->msg, item->valuestring, sizeof(out->msg) - 1);
        out->msg[sizeof(out->msg) - 1] = '\0';
        matrixify(out->msg);
    }

    /* 会话条目 */
    cJSON *entries = cJSON_GetObjectItem(doc, "entries");
    if (cJSON_IsArray(entries)) {
        uint8_t n = 0;
        int arr_size = cJSON_GetArraySize(entries);
        for (int i = 0; i < arr_size && n < 8; i++) {
            cJSON *v = cJSON_GetArrayItem(entries, i);
            if (cJSON_IsString(v)) {
                strncpy(out->lines[n], v->valuestring, 91);
                out->lines[n][91] = '\0';
                n++;
            }
        }
        if (n != out->n_lines || (n > 0 && strcmp(out->lines[n - 1], out->msg) != 0)) {
            out->line_gen++;
        }
        out->n_lines = n;
    }

    /* 审批提示 */
    cJSON *prompt = cJSON_GetObjectItem(doc, "prompt");
    if (cJSON_IsObject(prompt)) {
        cJSON *pid = cJSON_GetObjectItem(prompt, "id");
        cJSON *pt  = cJSON_GetObjectItem(prompt, "tool");
        cJSON *ph  = cJSON_GetObjectItem(prompt, "hint");
        if (pid && cJSON_IsString(pid)) {
            strncpy(out->prompt_id, pid->valuestring, sizeof(out->prompt_id) - 1);
            out->prompt_id[sizeof(out->prompt_id) - 1] = '\0';
        }
        if (pt && cJSON_IsString(pt)) {
            strncpy(out->prompt_tool, pt->valuestring, sizeof(out->prompt_tool) - 1);
            out->prompt_tool[sizeof(out->prompt_tool) - 1] = '\0';
            matrixify(out->prompt_tool);
        }
        if (ph && cJSON_IsString(ph)) {
            strncpy(out->prompt_hint, ph->valuestring, sizeof(out->prompt_hint) - 1);
            out->prompt_hint[sizeof(out->prompt_hint) - 1] = '\0';
            matrixify(out->prompt_hint);
        }
    } else {
        out->prompt_id[0] = '\0';
        out->prompt_tool[0] = '\0';
        out->prompt_hint[0] = '\0';
    }

    out->last_updated_ms = esp_timer_get_time() / 1000;
    s_runtime.last_live_ms = out->last_updated_ms;
    out->connected = true;

    cJSON_Delete(doc);
}

void buddy_apply_json_line(const char *line, ClaudeState *out) {
    apply_json(line, out);
}

/* 行缓冲 feed */
static void linebuf_feed(LineBuf *lb, const char *data, size_t len, ClaudeState *out) {
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\n' || c == '\r') {
            if (lb->len > 0) {
                lb->buf[lb->len] = '\0';
                if (lb->buf[0] == '{') apply_json(lb->buf, out);
                lb->len = 0;
            }
        } else if (lb->len < LINE_BUF_SIZE - 1) {
            lb->buf[lb->len++] = c;
        }
    }
}

/* 从 USB 轮询数据 */
void buddy_data_poll(ClaudeState *out) {
    uint32_t now_ms = esp_timer_get_time() / 1000;

    /* Demo 模式 */
    if (s_runtime.demo_mode) {
        if (now_ms >= s_runtime.demo_next_ms) {
            s_runtime.demo_idx = (s_runtime.demo_idx + 1) % FAKE_COUNT;
            s_runtime.demo_next_ms = now_ms + 8000;
        }
        const FakeScenario *f = &FAKES[s_runtime.demo_idx];
        out->sessions_total = f->total;
        out->sessions_running = f->running;
        out->sessions_waiting = f->waiting;
        out->recently_completed = f->completed;
        out->tokens_today = f->tokens;
        out->last_updated_ms = now_ms;
        out->connected = true;
        snprintf(out->msg, sizeof(out->msg), "demo: %s", f->name);
        return;
    }

    /* USB 数据由 uart_comm 通过回调传入 */
}

/* 外部供 uart_comm 调用 */
void buddy_feed_usb_line(const char *data, size_t len) {
    linebuf_feed(&s_usb_line, data, len, &s_claude);
}

bool buddy_data_connected(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    return s_runtime.last_live_ms != 0 && (now - s_runtime.last_live_ms) <= 30000;
}

const char* buddy_scenario_name(void) {
    if (s_runtime.demo_mode) return FAKES[s_runtime.demo_idx].name;
    if (buddy_data_connected()) return "usb";
    return "none";
}

void buddy_set_demo(bool on) {
    s_runtime.demo_mode = on;
    if (on) {
        s_runtime.demo_idx = 0;
        s_runtime.demo_next_ms = esp_timer_get_time() / 1000;
    }
}

bool buddy_is_demo(void) {
    return s_runtime.demo_mode;
}

bool buddy_rtc_valid(void) { return s_runtime.rtc_valid; }
void buddy_set_rtc_valid(bool v) { s_runtime.rtc_valid = v; }

bool buddy_has_pending_prompt(void) {
    return s_claude.prompt_id[0] != '\0' && !s_runtime.response_sent;
}

void buddy_clear_prompt(void) {
    s_claude.prompt_id[0] = '\0';
    s_runtime.response_sent = false;
}

void buddy_send_cmd(const char *json) {
    /* 发送到 USB */
    uart_send_raw(json, strlen(json));
    uart_send_raw("\n", 1);
}

/* 获取当前本地时间
 * 优先使用网络时间（SNTP 同步后的系统 RTC），回退到上位机同步的软件 RTC */
bool buddy_get_local_time(int *hour, int *min, int *sec,
                          int *year, int *month, int *day, int *dow) {
    /* 1) 系统时间有效（SNTP 已同步，time() 返回真实 epoch）
     *    2020-09-13 (epoch 1600000000) 之后视为有效 */
    time_t now = time(NULL);
    if (now > 1600000000) {
        struct tm lt;
        localtime_r(&now, &lt);   /* 使用 TZ 设置的本地时区 */

        if (hour)  *hour  = lt.tm_hour;
        if (min)   *min   = lt.tm_min;
        if (sec)   *sec   = lt.tm_sec;
        if (year)  *year  = lt.tm_year + 1900;
        if (month) *month = lt.tm_mon + 1;
        if (day)   *day   = lt.tm_mday;
        if (dow)   *dow   = lt.tm_wday;
        return true;
    }

    /* 2) 回退：上位机同步的软件 RTC */
    if (!s_runtime.rtc_valid) return false;

    uint32_t now_ms = esp_timer_get_time() / 1000;
    time_t local = (time_t)(s_rtc.epoch_sec + s_rtc.tz_offset_sec + (now_ms - s_rtc.set_at_ms) / 1000);

    struct tm lt;
    gmtime_r(&local, &lt);

    if (hour)  *hour  = lt.tm_hour;
    if (min)   *min   = lt.tm_min;
    if (sec)   *sec   = lt.tm_sec;
    if (year)  *year  = lt.tm_year + 1900;
    if (month) *month = lt.tm_mon + 1;
    if (day)   *day   = lt.tm_mday;
    if (dow)   *dow   = lt.tm_wday;
    return true;
}
