# Elara UI 重做 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 `docs/2026-10-11-ui-redesign-design.md` 把 Elara 固件 UI 重做为 Warm 暖棕配色、中文界面、宠物常驻 + 手势抽屉结构。

**Architecture:** 7 个递进任务：先落地中文字体流水线（其余任务都依赖），再建主题常量与中文文案，然后重做主界面，新增抽屉组件，改造导航（手势 + BOOT 键 + 菜单枚举），精简设置枚举，最后重做子页面并清理遗留代码。每个任务结束时固件必须能构建通过。

**Tech Stack:** ESP-IDF 6.1 / LVGL 8.4 / cJSON / `lv_font_conv` (npx) / Python 3

**Spec:** `docs/2026-10-11-ui-redesign-design.md`

## Global Constraints

- 构建必须在**同一条 shell 命令**内激活环境：`source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build`
- 每个任务结束要求 `idf.py build` **0 error / 0 warning**
- 屏幕尺寸固定 170×320，不改显示驱动
- 颜色常量使用 `lv_color_hex(0xRRGGBB)`
- 字体档位固定 **12 / 14 / 20 px**，bpp=1，不压缩；命名 `lv_font_sc_12` / `_14` / `_20`
- 不改动通信协议（`uart_comm` / `tcp_server` / `xfer` 报文格式）
- 不改动 `wifi_manager` / `storage_manager` / `battery` 模块行为，UI 层只调用现有 API
- 中文 UTF-8 字面量：缓冲区一律用 `sizeof(buf)-1`，防止 `format-truncation`
- 不新增第三方依赖（`lv_font_conv` 经 `npx` 调用，不写入 `package.json`）

## Pre-flight Rulings（计划冲突裁定）

评审计划时发现设计文档未明确、但实现必然要决断的 4 处冲突。按「spec 是权威、plan 是其论证」原则裁定如下，执行者按此执行：

- **R1 抽屉挂载点**：抽屉组件挂到 `buddy_ui_get_container()` 返回的容器，**不是** `lv_scr_act()`。理由：`buddy_ui.c` 已在 `lv_scr_act()` 上建了 `s_container`，抽屉必须与之同层才能正确覆盖页面。代价：若容器坐标非 (0,0) 需额外对齐。
- **R2 HUD 移除**：设计文档 §5.1 用「状态词」替代 HUD 面板，故删除 `buddy_ui_set_hud_text()` / `buddy_ui_set_hud_visible()` 及其全部调用点；`BUDDY_SET_HUD` 语义改为「信息面板」开关。代价：`main.c` 中 6 处 HUD 调用需改写。
- **R3 显示模式收敛**：`BUDDY_MODE_PET`（用量页）删除；`BUDDY_MODE_INFO` 由 7 页收敛为单一「关于」页。理由：用量已无数据源；Info 其余内容并入信息面板/关于。代价：`DISP_*` 状态机与 `INFO_PAGE_*` 枚举需同步收缩。
- **R4 设置项移除**：删除 `BUDDY_SET_LED`（无硬件）、`BUDDY_SET_ASCII`（物种切换移交宠物图鉴）。理由：设计文档 §5.4 明确移除。代价：`main.c` 中 `execute_settings_action` 与初始化处的对应分支必须同步删除。

---

### Task 1: 中文字体流水线

**Files:**
- Rewrite: `tools/generate_font.py`
- Create: `main/font/lv_font_sc_12.c`、`lv_font_sc_14.c`、`lv_font_sc_20.c`（脚本生成）
- Modify: `main/CMakeLists.txt`、`components/lv_conf.h`、`Makefile`、`AGENTS.md`

**Interfaces:**
- Consumes: 无（起点）
- Produces: LVGL 字体对象 `lv_font_sc_12` / `lv_font_sc_14` / `lv_font_sc_20`

- [ ] **Step 1: 重写 tools/generate_font.py**

```python
#!/usr/bin/env python3
"""中文字体流水线：源码提取字符集 -> lv_font_conv -> LVGL C 源文件。"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MAIN_DIR = REPO / "main"
FONT_DIR = MAIN_DIR / "font"

# ASCII/符号优先用等宽字体，保证 buddy_anim 的字符画对齐
MONO_FONT = "/System/Library/Fonts/Menlo.ttc"
CJK_FONT = "/System/Library/Fonts/Hiragino Sans GB.ttc"

STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')

# 预留常用字，减少改文案就重跑字体的频率
EXTRA_CHARS = (
    "待机工作中睡眠完成需要确认晕了开心设置显示连接系统亮度信息面板宠物模式旋转"
    "声音自动休眠恢复出厂关于关机网络已连接信号强弱可用上一个下一个左右滑动切换"
    "上滑菜单下滑信息年月日周六周天版本电量存储运行固件分钟小时充电中未连接时间"
    "未同步按键触摸导航返回帮助演示致谢设备芯片内存闪存屏幕电池电压温度状态"
)


def extract_charset() -> str:
    chars = set(chr(c) for c in range(0x20, 0x7F))
    for path in list(MAIN_DIR.rglob("*.c")) + list(MAIN_DIR.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for literal in STRING_RE.findall(text):
            for ch in literal:
                if ord(ch) > 0x7F:
                    chars.add(ch)
    chars.update(EXTRA_CHARS)
    return "".join(sorted(chars))


def build_font(size: int, charset: str, out_dir: Path) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"lv_font_sc_{size}.c"
    cmd = [
        "npx", "--yes", "lv_font_conv",
        "--size", str(size),
        "--bpp", "1",
        "--no-compress",
        "--format", "lvgl",
        "--lv-font-name", f"lv_font_sc_{size}",
        "--font", MONO_FONT, "--range", "0x20-0x7E",
        "--font", CJK_FONT, "--symbols", charset,
        "-o", str(out),
    ]
    subprocess.run(cmd, check=True, cwd=REPO)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sizes", default="12,14,20")
    args = ap.parse_args()
    charset = extract_charset()
    print(f"字符集: {len(charset)} 个字符")
    total = 0
    for size in [int(s) for s in args.sizes.split(",") if s.strip()]:
        out = build_font(size, charset, FONT_DIR)
        n = out.stat().st_size
        total += n
        print(f"  {out.name}: {n/1024:.1f} KB")
    print(f"合计: {total/1024:.1f} KB")
    if total > 60 * 1024:
        print("警告: 字体合计超过 60KB，请检查字符集", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: 生成字体**

Run: `cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && python3 tools/generate_font.py`
Expected: 打印 `字符集: N 个字符`（N 约 400–900）与三个文件 KB 数；三个 `.c` 文件存在且非空。

- [ ] **Step 3: 核对体积**

Run: `cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && du -ch main/font/*.c | tail -1`
Expected: 合计 **< 60 KB**（设计目标 <40KB）。超标则缩小 `EXTRA_CHARS` 后重跑。

- [ ] **Step 4: 注册到构建与 lv_conf**

`main/CMakeLists.txt` 的 `SRCS`，在 `"buddy/buddy_stats.c"` 后加：

```cmake
        "font/lv_font_sc_12.c"
        "font/lv_font_sc_14.c"
        "font/lv_font_sc_20.c"
```

`components/lv_conf.h` 字体段改为：

```c
/* 字体配置 */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1

/* 中文字体（由 tools/generate_font.py 生成） */
extern const lv_font_t lv_font_sc_12;
extern const lv_font_t lv_font_sc_14;
extern const lv_font_t lv_font_sc_20;

#define LV_FONT_DEFAULT &lv_font_sc_14
```

- [ ] **Step 5: 更新 Makefile 与 AGENTS.md**

`Makefile` 的 `font` 目标改为：

```makefile
# 生成中文字体（需要 node/npx）
font:
	python3 tools/generate_font.py
```

`AGENTS.md`「开发注意事项」末尾追加：

```
- 中文字体: 由 `tools/generate_font.py` 提取源码字符集后用 `npx lv_font_conv` 生成（`main/font/lv_font_sc_{12,14,20}.c`）。**新增中文字面量后必须重跑 `make font`**，否则新字显示为方块。
```

- [ ] **Step 6: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|binary size|Project build complete'`
Expected: `Project build complete`，无 `error` / `warning:`。

- [ ] **Step 7: Commit**

```bash
git add tools/generate_font.py main/font main/CMakeLists.txt components/lv_conf.h Makefile AGENTS.md
git commit -m "feat(font): 中文字体流水线，生成 12/14/20 三档点阵字体"
```

---

### Task 2: 主题常量与中文文案

**Files:**
- Create: `main/ui/buddy/buddy_theme.h`
- Create: `main/ui/buddy/buddy_strings.h`、`main/ui/buddy/buddy_strings.c`
- Modify: `main/CMakeLists.txt`（加入 `buddy_strings.c`）
- Modify: `main/ui/buddy/buddy_ui.c`（改用新常量）

**Interfaces:**
- Consumes: Task 1 的字体对象
- Produces:
  - `buddy_theme.h`: `COLOR_BG/PANEL/PANEL2/ACCENT/TEXT/TEXT_DIM/LINE/WARM/GREEN/RED`、`FONT_SM/MD/LG`、`SCREEN_W/SCREEN_H/STATUS_BAR_H/PAGE_PAD_X/CARD_RADIUS/ROW_H`
  - `buddy_strings.h`: `const char *buddy_persona_label(PersonaState s)`

- [ ] **Step 1: 创建 buddy_theme.h**

```c
/**
 * @file buddy_theme.h
 * Warm 暖棕配色 + 字号/间距规范（见 docs/2026-10-11-ui-redesign-design.md §6）
 */
#ifndef BUDDY_THEME_H
#define BUDDY_THEME_H

#include "lvgl.h"

/* 配色 (Warm) */
#define COLOR_BG        lv_color_hex(0x16110D)  /* 暖黑底 */
#define COLOR_PANEL     lv_color_hex(0x2A211A)  /* 卡片 */
#define COLOR_PANEL2    lv_color_hex(0x372A1F)  /* 选中/高亮 */
#define COLOR_ACCENT    lv_color_hex(0xFFB454)  /* 琥珀主色 */
#define COLOR_TEXT      lv_color_hex(0xFFF6EA)  /* 主文字 */
#define COLOR_TEXT_DIM  lv_color_hex(0x9C7F63)  /* 次要文字 */
#define COLOR_LINE      lv_color_hex(0x3E3023)  /* 分隔线 */
#define COLOR_WARM      lv_color_hex(0xFF8A7A)  /* 脸红/警示 */

/* 状态语义色 */
#define COLOR_GREEN     lv_color_hex(0x00FF00)
#define COLOR_RED       lv_color_hex(0xFF0000)

/* 字号：LVGL 点阵字体固定尺寸，3 档 */
#define FONT_SM         (&lv_font_sc_12)
#define FONT_MD         (&lv_font_sc_14)
#define FONT_LG         (&lv_font_sc_20)

/* 屏幕与布局 */
#define SCREEN_W        170
#define SCREEN_H        320
#define STATUS_BAR_H    24
#define PAGE_PAD_X      13
#define CARD_RADIUS     10
#define ROW_H           20

#endif /* BUDDY_THEME_H */
```

- [ ] **Step 2: 创建 buddy_strings.h / .c**

`main/ui/buddy/buddy_strings.h`：

```c
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
```

`main/ui/buddy/buddy_strings.c`：

```c
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
```

`main/CMakeLists.txt` 的 `SRCS` 加入 `"ui/buddy/buddy_strings.c"`。

- [ ] **Step 3: buddy_ui.c 改用新常量**

删除 `main/ui/buddy/buddy_ui.c` 顶部 `COLOR_*` 定义块与 `SCREEN_W`/`SCREEN_H` 定义，改为：

```c
#include "buddy_theme.h"
#include "buddy_strings.h"
```

保留仍被使用的 `ANIM_AREA_H` / `STATUS_AREA_Y`；删除已无引用的 `BAR_H`。

- [ ] **Step 4: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 5: Commit**

```bash
git add main/ui/buddy/buddy_theme.h main/ui/buddy/buddy_strings.h main/ui/buddy/buddy_strings.c main/ui/buddy/buddy_ui.c main/CMakeLists.txt
git commit -m "feat(ui): Warm 主题常量与中文状态词映射"
```

---

### Task 3: 主界面重做（状态栏 / 角色 / 状态词 / 心情点）

**Files:**
- Modify: `main/ui/buddy/buddy_ui.c`（`create_normal_page()`）
- Modify: `main/ui/buddy/buddy_ui.h`（声明 `buddy_ui_set_persona`）
- Modify: `main/main.c`（调用 `buddy_ui_set_persona`）

**Interfaces:**
- Consumes: Task 2 的 `buddy_theme.h` / `buddy_persona_label()`
- Produces: `void buddy_ui_set_persona(uint8_t persona_state)`

- [ ] **Step 1: 新增状态词与心情点控件**

在 `buddy_ui.c` 静态变量区新增：

```c
static lv_obj_t *s_persona_label = NULL;
static lv_obj_t *s_mood_dots[3] = {NULL, NULL, NULL};
```

在 `create_normal_page()` 中，`s_battery_label` 创建之后，**删除**原 `s_hud_panel` / `s_hud_label` 创建代码，替换为：

```c
    /* 状态栏分隔线 */
    lv_obj_t *sep = lv_obj_create(s_normal_page);
    lv_obj_set_size(sep, SCREEN_W - 12, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, STATUS_BAR_H - 1);

    /* 状态词 */
    s_persona_label = lv_label_create(s_normal_page);
    lv_label_set_text(s_persona_label, "待机");
    lv_obj_set_style_text_color(s_persona_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_persona_label, FONT_MD, 0);
    lv_obj_align(s_persona_label, LV_ALIGN_TOP_MID, 0, 202);

    /* 心情指示点 */
    for (int i = 0; i < 3; i++) {
        s_mood_dots[i] = lv_obj_create(s_normal_page);
        lv_obj_set_size(s_mood_dots[i], 5, 5);
        lv_obj_set_style_radius(s_mood_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_mood_dots[i], COLOR_LINE, 0);
        lv_obj_set_style_border_width(s_mood_dots[i], 0, 0);
        lv_obj_clear_flag(s_mood_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(s_mood_dots[i], LV_ALIGN_TOP_MID, (i - 1) * 12, 276);
    }

    /* 手势提示 */
    lv_obj_t *hint = lv_label_create(s_normal_page);
    lv_label_set_text(hint, "上滑菜单  下滑信息");
    lv_obj_set_style_text_color(hint, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(hint, FONT_SM, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 292);
```

- [ ] **Step 2: 实现 buddy_ui_set_persona()**

在 `buddy_ui.c` 的 `buddy_ui_anim_tick()` 之前新增：

```c
void buddy_ui_set_persona(uint8_t persona_state) {
    if (s_persona_label) {
        lv_label_set_text(s_persona_label, buddy_persona_label((PersonaState)persona_state));
        lv_obj_set_style_text_color(s_persona_label,
            persona_state == PERSONA_IDLE ? COLOR_TEXT_DIM : COLOR_TEXT, 0);
    }
    if (s_mood_dots[1]) {
        bool active = persona_state != PERSONA_IDLE;
        lv_obj_set_style_bg_color(s_mood_dots[1], active ? COLOR_ACCENT : COLOR_LINE, 0);
    }
}
```

`buddy_ui.h`「主页 (NORMAL)」段声明：

```c
/**
 * 更新主界面 Persona 状态词与心情指示
 * @param persona_state PersonaState 枚举值
 */
void buddy_ui_set_persona(uint8_t persona_state);
```

- [ ] **Step 3: 移除 HUD API 及其调用点（R2）**

删除 `buddy_ui.c` 中 `buddy_ui_set_hud_text()` / `buddy_ui_set_hud_visible()` 的实现，以及 `buddy_ui.h` 中的声明和 `s_hud_panel` / `s_hud_label` 静态变量。

改写 `main.c` 的调用点：

| 位置 | 原调用 | 改为 |
|---|---|---|
| 渲染段 HUD 更新（约 281 行） | `buddy_ui_set_hud_text(session->lines[...])` / `(session->msg)` | 删除该段（状态词已替代） |
| `BUDDY_MENU_HELP` 分支 | `buddy_ui_set_hud_text("Short=nav  Long=back")` + `set_hud_visible(true)` | 删除（Help 并入关于页，见 Task 5） |
| `BUDDY_MENU_DEMO` 分支 | `buddy_ui_set_hud_text(...)` + `set_hud_visible(true)` | 删除（Demo 并入关于页，见 Task 5） |
| `BUDDY_SET_HUD` 分支 | `buddy_ui_set_hud_visible(set->hud)` | 删除（该设置语义改为信息面板，见 Task 6） |
| 欢迎消息（约 723 行） | `buddy_ui_set_hud_text(line)` / `("Hello!")` + `set_hud_visible(true)` | 删除 HUD 调用，保留 `s_welcome_until_ms` 计时 |

删除后确认 `s_welcome_until_ms` 若已无读取方，则一并删除该变量与其赋值。

- [ ] **Step 4: main.c 调用状态词更新**

在渲染段 `buddy_ui_anim_set_persona(rt->active_state);` 之后加入：

```c
            buddy_ui_set_persona((uint8_t)rt->active_state);
```

- [ ] **Step 5: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 6: Commit**

```bash
git add main/ui/buddy/buddy_ui.c main/ui/buddy/buddy_ui.h main/main.c
git commit -m "feat(ui): 主界面重做 - 状态栏/状态词/心情点，移除 HUD"
```

---

### Task 4: 修复 LVGL 配置源（中文字体默认生效）【插入任务 · 裁定 R5】

**背景（控制器实测结论，均已验证）**

1. `CONFIG_LV_CONF_SKIP=y` 是 LVGL Kconfig 默认值 → `components/lv_conf.h` **完全不被解析**。
2. 把 `CONFIG_LV_CONF_SKIP` 改为 n 会**直接构建失败**：
   `lv_conf_internal.h:39: fatal error: lv_conf.h: No such file or directory`
   （LVGL 组件不在 `components/` 的 include 路径上）。
3. `LV_FONT_DEFAULT` 是 Kconfig 的 **choice**，无法指向自定义字体。
4. **`CONFIG_LV_FONT_CUSTOM_DECLARE` 方案不可行**（实测）：Kconfig string 在 `sdkconfig.h` 中
   变成 C 字符串字面量 `"LV_FONT_DECLARE(...)"`，而 `lv_font.h:244-245` 会把它作为**语句**展开 ——
   字符串字面量语句是 no-op，**不声明任何东西**。
5. 因此唯一可行路径：**应用层**定义 `BUDDY_FONT_DEFAULT`，替换我们 UI 代码里的 `LV_FONT_DEFAULT`。
   （LVGL 内部默认字体仍是 Montserrat 14，但我们所有控件都显式设置字体，故不受影响。）

**Files:**
- Modify: `main/ui/buddy/buddy_theme.h`（新增 `BUDDY_FONT_DEFAULT`）
- Modify: `main/ui/buddy/buddy_ui.c`（8 处替换）
- Modify: `main/ui/buddy/buddy_anim.c`（1 处替换 + include）
- Modify: `main/ui/lvgl_widgets.c`（4 处替换 + include + 删除冲突的本地颜色宏）
- Modify: `components/lv_conf.h`（仅加说明注释）
- Modify: `AGENTS.md`（记录配置真相源）

**Interfaces:**
- Consumes: Task 1 的 `lv_font_sc_12/14/20`
- Produces: `BUDDY_FONT_DEFAULT`（= `FONT_MD` = `lv_font_sc_14`）

- [ ] **Step 1: 在 buddy_theme.h 新增应用层默认字体**

在 `FONT_SM/MD/LG` 定义**之后**加入（注意：`BUDDY_FONT_DEFAULT` 依赖 `FONT_MD`，必须定义在其后）：

```c
/* 应用层默认字体：Kconfig 的 LV_FONT_DEFAULT 是 choice，无法指向自定义字体，
 * 故本工程统一以 FONT_MD 作为界面默认字体（见 docs/2026-10-11-ui-redesign-design.md §7）。 */
#define BUDDY_FONT_DEFAULT  FONT_MD
```

- [ ] **Step 2: 替换 buddy_ui.c 中的 LV_FONT_DEFAULT**

Run: `grep -c 'LV_FONT_DEFAULT' main/ui/buddy/buddy_ui.c`
Expected before: `8`

用 sed 或编辑器把这 8 处 `LV_FONT_DEFAULT` 全部改为 `BUDDY_FONT_DEFAULT`。

Run: `grep -c 'LV_FONT_DEFAULT' main/ui/buddy/buddy_ui.c`
Expected after: `0`

- [ ] **Step 3: 替换 buddy_anim.c（字符画用等宽）**

该文件的 ASCII 字符画标签使用 `LV_FONT_DEFAULT`。加 include 并替换：

```c
#include "buddy_theme.h"
```
```c
    lv_obj_set_style_text_font(label, FONT_MD, 0);
```
（原为 `LV_FONT_DEFAULT`）

- [ ] **Step 4: 替换 lvgl_widgets.c，并解决颜色宏冲突**

**注意：这一步有个已实测的坑。** `lvgl_widgets.c` 自己定义了
`COLOR_TEXT` / `COLOR_TEXT_DIM`，而 `buddy_theme.h` 也定义同名宏 ——
直接 include 会触发 `-Werror` 的 `macro redefined` 错误（实测报错：
`lvgl_widgets.c:15:9: error: 'COLOR_TEXT' redefined [-Werror]`）。

处理方式：**删除 `lvgl_widgets.c` 里与 `buddy_theme.h` 重名的两个本地宏**
（`COLOR_TEXT`、`COLOR_TEXT_DIM`），改为使用主题里的定义；保留其余仅本文件使用的宏
（`COLOR_USER_BUB` / `COLOR_AI_BUB` / `COLOR_SYS_BUB` / `COLOR_LABEL_AI` / `COLOR_LABEL_USER`）。

然后加 `#include "buddy_theme.h"`，并把 4 处 `LV_FONT_DEFAULT` 改为 `BUDDY_FONT_DEFAULT`。

Run: `grep -c 'LV_FONT_DEFAULT' main/ui/lvgl_widgets.c`
Expected after: `0`

- [ ] **Step 5: 说明 lv_conf.h 不生效 + 更新 AGENTS.md**

在 `components/lv_conf.h` 顶部注释加一行：

```c
 * 注意：本工程 LVGL 以 Kconfig 为准（CONFIG_LV_CONF_SKIP=y），本文件不参与编译。
 *       字体配置见 sdkconfig.defaults 与 main/ui/buddy/buddy_theme.h。
```

`AGENTS.md`「开发注意事项」追加：

```
- LVGL 配置真相源是 `sdkconfig`（Kconfig），`components/lv_conf.h` **不参与编译**
  （`CONFIG_LV_CONF_SKIP=y`，且 LVGL 组件看不到该路径）。界面默认字体用
  `buddy_theme.h` 的 `BUDDY_FONT_DEFAULT`，不要用 LVGL 的 `LV_FONT_DEFAULT`（那是不含中文的 Montserrat）。
```

- [ ] **Step 6: 构建验证**

```bash
source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete|binary size'
```
Expected: `Project build complete`，0 error / 0 warning。

- [ ] **Step 7: 全仓残留自检**

Run: `grep -rn 'LV_FONT_DEFAULT' main/ | grep -v lvgl_chat_ui`
Expected: 无输出（`lvgl_chat_ui.c` 待 Task 8 删除，可豁免）。

- [ ] **Step 8: Commit**

```bash
git add main/ui/buddy/buddy_theme.h main/ui/buddy/buddy_ui.c main/ui/buddy/buddy_anim.c main/ui/lvgl_widgets.c components/lv_conf.h AGENTS.md
git commit -m "fix(ui): 统一界面默认字体为 BUDDY_FONT_DEFAULT，修正 LVGL 配置认知"
```

> 注意：本任务**不改 `sdkconfig.defaults`**。之前尝试的 `CONFIG_LV_FONT_CUSTOM*` 方案已实测无效，
> 不要重新引入。

---

### Task 5: 抽屉组件与信息面板

**Files:**
- Create: `main/ui/buddy/buddy_drawer.h`、`main/ui/buddy/buddy_drawer.c`
- Modify: `main/CMakeLists.txt`
- Modify: `main/ui/buddy/buddy_ui.c`（初始化时调用）

**Interfaces:**
- Consumes: Task 2 `buddy_theme.h`；`battery_get_percentage()` / `battery_is_charging()`；`wifi_manager_get_signal_level()` / `wifi_manager_get_connected_ssid()`；`storage_get_info()`；`buddy_get_local_time()`
- Produces:
  - `void buddy_drawer_init(lv_obj_t *parent)`
  - `void buddy_drawer_open(bool info_panel)`
  - `void buddy_drawer_close(void)`
  - `bool buddy_drawer_is_open(void)`
  - `bool buddy_drawer_is_info_panel(void)`
  - `void buddy_drawer_refresh(void)`

- [ ] **Step 1: 创建 buddy_drawer.h**

```c
/**
 * @file buddy_drawer.h
 * 底部抽屉：信息面板（见设计文档 §4.1 / §5.2）
 */
#ifndef BUDDY_DRAWER_H
#define BUDDY_DRAWER_H

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @param parent 挂载父容器（应为 buddy_ui_get_container()，见 R1） */
void buddy_drawer_init(lv_obj_t *parent);
/** @param info_panel true=信息面板, false=菜单 */
void buddy_drawer_open(bool info_panel);
void buddy_drawer_close(void);
bool buddy_drawer_is_open(void);
bool buddy_drawer_is_info_panel(void);
void buddy_drawer_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_DRAWER_H */
```

- [ ] **Step 2: 实现 buddy_drawer.c**

```c
/**
 * @file buddy_drawer.c
 */
#include "buddy_drawer.h"
#include "buddy_theme.h"
#include "buddy/buddy_state.h"
#include "battery/battery_monitor.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "BUDDY_DRAWER";

#define DRAWER_H        232
#define DRAWER_RADIUS   12
#define INFO_ROWS       5

static lv_obj_t *s_scrim = NULL;
static lv_obj_t *s_panel = NULL;
static lv_obj_t *s_title = NULL;
static lv_obj_t *s_subtitle = NULL;
static lv_obj_t *s_row_val[INFO_ROWS] = {0};
static bool s_open = false;
static bool s_is_info = true;

static const char *ROW_KEYS[INFO_ROWS] = { "电量", "网络", "存储", "运行", "固件" };

static void panel_anim_cb(void *obj, int32_t y) {
    lv_obj_set_y((lv_obj_t *)obj, y);
}

void buddy_drawer_init(lv_obj_t *parent) {
    if (!parent) { ESP_LOGE(TAG, "null parent"); return; }

    s_scrim = lv_obj_create(parent);
    lv_obj_set_size(s_scrim, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_scrim, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_scrim, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_scrim, 0, 0);
    lv_obj_set_style_radius(s_scrim, 0, 0);
    lv_obj_set_style_pad_all(s_scrim, 0, 0);
    lv_obj_align(s_scrim, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);

    s_panel = lv_obj_create(s_scrim);
    lv_obj_set_size(s_panel, SCREEN_W, DRAWER_H);
    lv_obj_set_style_bg_color(s_panel, COLOR_PANEL, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, DRAWER_RADIUS, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_panel, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *handle = lv_obj_create(s_panel);
    lv_obj_set_size(handle, 28, 3);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, COLOR_LINE, 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 7);

    s_title = lv_label_create(s_panel);
    lv_label_set_text(s_title, "--");
    lv_obj_set_style_text_font(s_title, FONT_MD, 0);
    lv_obj_set_style_text_color(s_title, COLOR_TEXT, 0);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 24);

    s_subtitle = lv_label_create(s_panel);
    lv_label_set_text(s_subtitle, "--");
    lv_obj_set_style_text_font(s_subtitle, FONT_SM, 0);
    lv_obj_set_style_text_color(s_subtitle, COLOR_TEXT_DIM, 0);
    lv_obj_align(s_subtitle, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 43);

    lv_obj_t *sep = lv_obj_create(s_panel);
    lv_obj_set_size(sep, SCREEN_W - 2 * PAGE_PAD_X, 1);
    lv_obj_set_style_bg_color(sep, COLOR_LINE, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sep, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 58);

    for (int i = 0; i < INFO_ROWS; i++) {
        lv_obj_t *k = lv_label_create(s_panel);
        lv_label_set_text(k, ROW_KEYS[i]);
        lv_obj_set_style_text_font(k, FONT_SM, 0);
        lv_obj_set_style_text_color(k, COLOR_TEXT_DIM, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, PAGE_PAD_X, 66 + i * 21);

        s_row_val[i] = lv_label_create(s_panel);
        lv_label_set_text(s_row_val[i], "--");
        lv_obj_set_style_text_font(s_row_val[i], FONT_SM, 0);
        lv_obj_set_style_text_color(s_row_val[i], COLOR_TEXT, 0);
        lv_obj_align(s_row_val[i], LV_ALIGN_TOP_RIGHT, -PAGE_PAD_X, 66 + i * 21);
    }
    ESP_LOGI(TAG, "drawer initialized");
}

void buddy_drawer_refresh(void) {
    if (!s_open || !s_is_info) return;

    int h = 0, m = 0, sec = 0, y = 0, mo = 0, d = 0, dow = 0;
    if (buddy_get_local_time(&h, &m, &sec, &y, &mo, &d, &dow)) {
        static const char *WD[] = { "日", "一", "二", "三", "四", "五", "六" };
        lv_label_set_text_fmt(s_title, "%d月%d日 周%s", mo, d, WD[dow % 7]);
        lv_label_set_text_fmt(s_subtitle, "%d · %02d:%02d", y, h, m);
    } else {
        lv_label_set_text(s_title, "时间未同步");
        lv_label_set_text(s_subtitle, "--");
    }

    lv_label_set_text_fmt(s_row_val[0], "%d%%%s",
        battery_get_percentage(), battery_is_charging() ? " 充电中" : "");

    char ssid[33] = {0};
    if (wifi_manager_get_connected_ssid(ssid, sizeof(ssid))) {
        lv_label_set_text_fmt(s_row_val[1], "%s  %d/3", ssid, wifi_manager_get_signal_level());
    } else {
        lv_label_set_text(s_row_val[1], "未连接");
    }

    size_t total = 0, used = 0;
    if (storage_get_info(&total, &used) == ESP_OK && total > 0) {
        lv_label_set_text_fmt(s_row_val[2], "%.1f / %.1f MB",
            (double)used / 1048576.0, (double)total / 1048576.0);
    } else {
        lv_label_set_text(s_row_val[2], "--");
    }

    uint32_t up_s = (uint32_t)(esp_timer_get_time() / 1000000);
    lv_label_set_text_fmt(s_row_val[3], "%lu小时%lu分",
        (unsigned long)(up_s / 3600), (unsigned long)((up_s / 60) % 60));
    lv_label_set_text(s_row_val[4], "v1.0.0");
}

void buddy_drawer_open(bool info_panel) {
    s_is_info = info_panel;
    s_open = true;
    lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_scrim);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_panel);
    lv_anim_set_exec_cb(&a, panel_anim_cb);
    lv_anim_set_values(&a, DRAWER_H, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_start(&a);

    buddy_drawer_refresh();
}

void buddy_drawer_close(void) {
    s_open = false;
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
}

bool buddy_drawer_is_open(void) { return s_open; }
bool buddy_drawer_is_info_panel(void) { return s_is_info; }
```

- [ ] **Step 3: 注册到构建与初始化**

`main/CMakeLists.txt` 的 `SRCS` 加入 `"ui/buddy/buddy_drawer.c"`。

`main/ui/buddy/buddy_ui.c`：顶部加 `#include "buddy_drawer.h"`；在 `buddy_ui_init()` 末尾（容器创建之后）加入：

```c
    buddy_drawer_init(s_container);
```

- [ ] **Step 4: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 5: Commit**

```bash
git add main/ui/buddy/buddy_drawer.c main/ui/buddy/buddy_drawer.h main/CMakeLists.txt main/ui/buddy/buddy_ui.c
git commit -m "feat(ui): 新增抽屉组件与信息面板"
```

---

### Task 6: 导航改造（手势 + BOOT 键 + 菜单枚举）

**Files:**
- Modify: `main/ui/buddy/buddy_ui.c`（滑动回调改为公开函数）
- Modify: `main/ui/buddy/buddy_ui.h`（声明 `buddy_ui_handle_swipe`；精简 `BuddyMenuItem`）
- Modify: `main/main.c`（抽屉状态机、BOOT 键、`execute_menu_action` 适配新枚举）

**Interfaces:**
- Consumes: Task 4 的 `buddy_drawer_*`
- Produces: `void buddy_ui_handle_swipe(lv_dir_t dir)`

- [ ] **Step 1: 滑动回调改为公开函数**

`buddy_ui.c` 中把 `static void info_swipe_cb(lv_dir_t dir)` 替换为：

```c
void buddy_ui_handle_swipe(lv_dir_t dir) {
    /* Petdex 页面：左右切换动画状态 */
    if (pet_ui_is_visible()) {
        if (dir == LV_DIR_LEFT)  pet_ui_next_state();
        if (dir == LV_DIR_RIGHT) pet_ui_prev_state();
        return;
    }
    /* 抽屉打开时：上下滑动关闭 */
    if (buddy_drawer_is_open()) {
        if (dir == LV_DIR_BOTTOM || dir == LV_DIR_TOP) buddy_drawer_close();
        return;
    }
    /* 主界面：下滑出信息面板，上滑出菜单 */
    if (dir == LV_DIR_BOTTOM) buddy_drawer_open(true);
    else if (dir == LV_DIR_TOP) buddy_drawer_open(false);
}
```

`buddy_ui_init()` 中 `lvgl_touch_set_swipe_cb(info_swipe_cb);` 改为 `lvgl_touch_set_swipe_cb(buddy_ui_handle_swipe);`

`buddy_ui.h` 声明：

```c
/**
 * 处理触摸滑动（由 lvgl_touch 驱动层回调）
 * @param dir 滑动方向
 */
void buddy_ui_handle_swipe(lv_dir_t dir);
```

- [ ] **Step 2: 菜单枚举精简为 5 项**

`buddy_ui.h` 的 `BuddyMenuItem` 改为：

```c
typedef enum {
    BUDDY_MENU_PETDEX = 0,
    BUDDY_MENU_WIFI,
    BUDDY_MENU_SETTINGS,
    BUDDY_MENU_ABOUT,
    BUDDY_MENU_SHUTDOWN,
    BUDDY_MENU_MAX
} BuddyMenuItem;
```

`buddy_ui.c` 的 `s_menu_texts` 改为：

```c
static const char *s_menu_texts[BUDDY_MENU_MAX] = {
    LV_SYMBOL_IMAGE   " 宠物图鉴",
    LV_SYMBOL_WIFI    " 网络",
    LV_SYMBOL_SETTINGS" 设置",
    LV_SYMBOL_HOME    " 关于",
    LV_SYMBOL_POWER   " 关机",
};
```

- [ ] **Step 3: 适配 main.c 的 execute_menu_action**

重写 `main.c` 的 `execute_menu_action()` switch（**必须**同步，否则新枚举下 `BUDDY_MENU_HELP` 等不再存在会编译失败）：

```c
static void execute_menu_action(BuddyMenuItem item) {
    BuddyUIState *ui = buddy_get_ui_state();
    BuddyRuntime *rt = buddy_get_runtime();

    ui->menu_open = false;
    buddy_ui_show_menu(false);

    switch (item) {
    case BUDDY_MENU_PETDEX:
        pet_ui_show(true);
        break;
    case BUDDY_MENU_WIFI:
        wifi_ui_show(true);
        break;
    case BUDDY_MENU_SETTINGS:
        ui->settings_open = true;
        ui->settings_sel = 0;
        buddy_ui_show_settings(true);
        buddy_ui_settings_select(BUDDY_SET_BRIGHTNESS);
        break;
    case BUDDY_MENU_ABOUT:
        ui->display_mode = DISP_INFO;
        ui->info_page = INFO_PAGE_ABOUT;
        buddy_ui_set_mode(BUDDY_MODE_INFO);
        buddy_ui_set_info_page(INFO_PAGE_ABOUT);
        break;
    case BUDDY_MENU_SHUTDOWN:
        ESP_LOGI(TAG, "Shutdown requested");
        backlight_set_on(false);
        rt->screen_off = true;
        esp_sleep_enable_ext0_wakeup(BOOT_KEY_PIN, 0);
        esp_deep_sleep_start();
        break;
    default:
        buddy_anim_invalidate();
        break;
    }
}
```

> Demo 入口：在「关于」页内长按触发（Task 6 实现），此处不再单独处理。

- [ ] **Step 4: BOOT 键与抽屉状态机**

`main.c` 顶部加 `#include "ui/buddy/buddy_drawer.h"`。

在 `process_boot_key()` 的**释放**处理里，长按与短按分支最前面各加抽屉优先处理：

```c
        if (buddy_drawer_is_open()) {
            if (press_dur >= 600) {
                if (lv_port_disp_lock(-1)) { buddy_drawer_close(); lv_port_disp_unlock(); }
            }
            return;
        }
```

把主界面短按的「循环 NORMAL→PET→INFO」整段替换为打开菜单：

```c
            } else {
                if (lv_port_disp_lock(-1)) {
                    ui->menu_open = true;
                    ui->menu_sel = 0;
                    buddy_ui_show_menu(true);
                    lv_port_disp_unlock();
                }
            }
```

> 说明：长按主界面原本已打开菜单，保持；短按改为同样打开菜单，模式循环取消。

- [ ] **Step 5: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 6: Commit**

```bash
git add main/ui/buddy/buddy_ui.c main/ui/buddy/buddy_ui.h main/main.c
git commit -m "feat(ui): 手势抽屉导航、BOOT 键与菜单项精简"
```

---

### Task 7: 设置精简与显示模式收敛

**Files:**
- Modify: `main/ui/buddy/buddy_ui.h`（`BuddySettingItem` 精简；`InfoPageIdx` 收敛；`BuddyMode` 收敛）
- Modify: `main/ui/buddy/buddy_ui.c`（设置文案与分组、Info 页收敛）
- Modify: `main/main.c`（`execute_settings_action` 与初始化处适配）

**Interfaces:**
- Consumes: Task 2 主题/文案、Task 5 的菜单枚举
- Produces: 精简后的设置枚举与单一「关于」页

- [ ] **Step 1: 精简 BuddySettingItem（R4）**

`buddy_ui.h` 改为：

```c
typedef enum {
    BUDDY_SET_BRIGHTNESS = 0,
    BUDDY_SET_INFO_PANEL,   /* 信息面板开关（原 HUD） */
    BUDDY_SET_PET_MODE,
    BUDDY_SET_ROTATE,
    BUDDY_SET_WIFI,
    BUDDY_SET_SOUND,
    BUDDY_SET_AUTO_SLEEP,
    BUDDY_SET_RESET,
    BUDDY_SET_BACK,
    BUDDY_SET_MAX
} BuddySettingItem;
```

`buddy_ui.c` 的 `s_setting_texts` 同步改为：

```c
static const char *s_setting_texts[BUDDY_SET_MAX] = {
    LV_SYMBOL_IMAGE     " 亮度",
    LV_SYMBOL_EYE_OPEN  " 信息面板",
    LV_SYMBOL_VIDEO     " 宠物模式",
    LV_SYMBOL_REFRESH   " 旋转",
    LV_SYMBOL_WIFI      " Wi-Fi",
    LV_SYMBOL_VOLUME_MAX" 声音",
    LV_SYMBOL_CHARGE    " 自动休眠",
    LV_SYMBOL_TRASH     " 恢复出厂",
    LV_SYMBOL_LEFT      " 返回",
};
```

- [ ] **Step 2: 适配 main.c 的设置分支**

重写 `execute_settings_action()` 的 switch，删除 `BUDDY_SET_LED` / `BUDDY_SET_ASCII` / `BUDDY_SET_HUD` 分支，新增 `BUDDY_SET_INFO_PANEL`：

```c
    case BUDDY_SET_INFO_PANEL: {
        BuddySettings *set = buddy_settings_get();
        set->hud = !set->hud;                 /* 复用 hud 字段作为信息面板开关 */
        buddy_ui_settings_set_toggle(BUDDY_SET_INFO_PANEL, set->hud);
        buddy_settings_save();
        break;
    }
```

其余分支保持，但 `BUDDY_SET_HUD` 的引用全部改名。同时修正 `app_main` 初始化处（约 682 行）的 `buddy_ui_settings_set_toggle(BUDDY_SET_LED, ...)` 与 `(BUDDY_SET_HUD, ...)`：

```c
    buddy_ui_settings_set_toggle(BUDDY_SET_INFO_PANEL, set->hud);
```

（`BUDDY_SET_LED` 那行删除。）

- [ ] **Step 3: Info 页收敛为单一「关于」页（R3）**

`buddy_ui.h` 的 `InfoPageIdx` 收敛：

```c
typedef enum {
    INFO_PAGE_ABOUT = 0,
    INFO_PAGE_MAX
} InfoPageIdx;
```

`BuddyMode` 收敛（删除 PET 模式）：

```c
typedef enum {
    BUDDY_MODE_NORMAL = 0,
    BUDDY_MODE_INFO,
    BUDDY_MODE_MAX
} BuddyMode;
```

`buddy_ui.c` 的 `s_info_titles` 改为 `{ "关于" }`；`update_info_content()` 只保留 `INFO_PAGE_ABOUT` 分支，正文改为中文（版本/设备/按键说明）：

```c
    case INFO_PAGE_ABOUT: {
        lv_label_set_text(s_info_content,
            "Elara 桌面伴侣\n"
            "============\n"
            "版本: 1.0.0\n"
            "LVGL: 8.4\n"
            "芯片: ESP32-S3\n"
            "屏幕: 170x320\n\n"
            "按键说明:\n"
            "  BOOT 短按 = 菜单\n"
            "  BOOT 长按 = 返回\n"
            "  触摸 = 导航");
        break;
    }
```

删除 `create_pet_page()` 与 `INFO_PAGE_BUTTONS/SESSION/DEVICE/NETWORK/BATTERY/CREDITS` 相关代码及 `update_info_session_content()`。`main.c` 中 `DISP_PET` 的引用同步删除（`DispMode` 收敛为 NORMAL/INFO）。

- [ ] **Step 4: 关于页长按进入 Demo**

在 `buddy_ui.c` 的 Info 页内容区注册长按回调：

```c
    lv_obj_add_event_cb(s_info_content, info_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
```

```c
static void info_long_press_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    extern void buddy_set_demo(bool on);
    extern bool buddy_is_demo(void);
    buddy_set_demo(!buddy_is_demo());
    lv_label_set_text(s_info_content, buddy_is_demo() ? "演示模式: 开" : "演示模式: 关");
}
```

- [ ] **Step 5: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 6: Commit**

```bash
git add main/ui/buddy/buddy_ui.c main/ui/buddy/buddy_ui.h main/main.c
git commit -m "feat(ui): 设置项精简、显示模式收敛为单一关于页"
```

---

### Task 8: 子页面重做、中文化与遗留清理

**Files:**
- Modify: `main/ui/pet_ui.c`（配色 + 中文化）
- Modify: `main/ui/wifi_ui.c`（配色 + 中文化）
- Delete: `main/ui/lvgl_chat_ui.c`、`main/ui/lvgl_chat_ui.h`
- Modify: `AGENTS.md`（模块清单）
- Modify: `test/test_lvgl.c`（移除对已删除头文件的引用）

**Interfaces:**
- Consumes: Task 2 `buddy_theme.h`
- Produces: 全中文界面，无遗留英文 UI 字符串

- [ ] **Step 1: pet_ui.c 配色与中文化**

加 `#include "buddy_theme.h"`，把硬编码颜色（如 `lv_color_hex(0xA0A0AA)`）替换为 `COLOR_TEXT_DIM` / `COLOR_PANEL` / `COLOR_ACCENT`；可见文案改为中文：`"PETDEX ANIMATION"` → `"宠物图鉴"`，`"State: Idle"` → `"状态: 待机"`，`" prev"` → `"上一个"`，`"next "` → `"下一个"`，`"Frame: %d / %d"` → `"帧 %d / %d"`。

- [ ] **Step 2: wifi_ui.c 配色与中文化**

加 `#include "buddy_theme.h"`，统一配色；把页面标题、连接状态、按钮文案改为中文（如 `"WiFi"` → `"网络"`，`"Connected"` → `"已连接"`，`"Scan"` → `"扫描"`，`"Connect"` → `"连接"`，`"Password"` → `"密码"`）。**不改动** `wifi_manager_*` API 调用逻辑。

- [ ] **Step 3: 删除遗留聊天 UI**

Run:
```bash
cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && git rm main/ui/lvgl_chat_ui.c main/ui/lvgl_chat_ui.h
```

`test/test_lvgl.c` 中删除 `#include "lvgl_chat_ui.h"` 及其相关测试用例（该头文件已不存在，否则测试组件编译失败）。

`AGENTS.md` 模块清单删除 `main/ui/lvgl_chat_ui.c` 一行。

- [ ] **Step 4: 构建验证**

Run: `source /Users/yulinling/.espressif/tools/activate_idf_v6.1.sh >/dev/null 2>&1 && cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && idf.py build 2>&1 | grep -iE 'error|warning:|Project build complete'`
Expected: 构建通过，0 warning。

- [ ] **Step 5: 重跑字体覆盖新文案**

Run: `cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && python3 tools/generate_font.py`
Expected: 字符集数量 ≥ 首次；三个字体文件重新生成；随后再跑一次构建确认通过。

- [ ] **Step 6: 结构校验与残留扫描**

Run:
```bash
cd /Users/yulinling/.codex/worktrees/ui-redesign/Elara && python3 tools/verify_structure.py 2>&1 | tail -4
grep -rn 'lvgl_chat_ui' main/ test/ AGENTS.md || echo "chat_ui CLEAN"
```
Expected: 结构校验通过；`chat_ui CLEAN`。

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat(ui): 子页面中文化与配色统一，删除遗留聊天 UI"
```

---

## 附: 任务依赖

```
Task 1 (字体) ──> Task 2 (主题/文案) ──┬─> Task 3 (主界面)
                                        └─> Task 4 (抽屉) ──> Task 5 (导航)
                                                                     │
                                          Task 6 (设置/模式) <────────┤
                                                                     ▼
                                                          Task 7 (子页面/清理)
```

Task 1 为全局前置。Task 2 依赖 Task 1。Task 3、4 依赖 Task 2。Task 5 依赖 Task 4。Task 6 依赖 Task 5。Task 7 依赖 Task 6。
