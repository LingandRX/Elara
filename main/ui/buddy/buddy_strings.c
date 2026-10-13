/**
 * @file buddy_strings.c
 */
#include "buddy_strings.h"

const char *buddy_persona_label(PersonaState s) {
    switch (s) {
    case PERSONA_SLEEP:     return "睡眠中";
    case PERSONA_IDLE:      return "待机";
    case PERSONA_BUSY:      return "工作中…";
    case PERSONA_ATTENTION: return "需要确认";
    case PERSONA_CELEBRATE: return "完成！";
    case PERSONA_DIZZY:     return "晕了";
    case PERSONA_HEART:     return "开心";
    default:                return "待机";
    }
}
