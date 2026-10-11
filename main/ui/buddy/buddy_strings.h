/**
 * @file buddy_strings.h
 * 界面中文文案与 Persona 状态词映射
 */
#ifndef BUDDY_STRINGS_H
#define BUDDY_STRINGS_H

#include "buddy/buddy_state.h"

/** Persona 状态 -> 中文状态词（见设计文档 §5.1） */
const char *buddy_persona_label(PersonaState s);

#endif /* BUDDY_STRINGS_H */
