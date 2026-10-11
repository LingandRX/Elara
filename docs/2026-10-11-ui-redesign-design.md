# Elara UI 重做设计（Warm 暖棕 + 中文 + ASCII）

> 状态: 已定稿，待实施
> 日期: 2026-10-11
> 适用硬件: ESP32-S3-Touch-LCD-1.9（170×320, RGB565, CST816T 单点触摸）
> 效果图: 见 [附录 A](#附录-a-效果图)

---

## 1. 背景与目标

### 1.1 触发原因

设备已移除全部 AI 编码工具对接（commit `7afcfb1`）与审批链路（commit `7a69393`）后，UI 暴露出四个问题：

| 问题 | 具体表现 |
|------|----------|
| **太挤** | 170×320 塞下 3 个显示模式 + 3 个覆盖层 + 7 个 Info 文本页；仅用 14px/16px 两档字号硬撑 |
| **太丑** | 主界面动画区占 170px，下半部是裸露文本行；配色（薄荷青）与信息层级不匹配 |
| **信息没用** | Info 页 7 页纯文本需短按循环 7 次才能翻到；用量条已无数据源 |
| **想换风格** | 现有深蓝黑 + 薄荷青偏"工程师风"，与桌面宠物定位不符 |

### 1.2 目标

1. **宠物为绝对主角**：主界面只保留角色 + 状态 + 状态栏
2. **信息收敛**：7 个 Info 文本页合并为一个下滑信息面板
3. **导航变浅**：3 模式循环改为「宠物常驻 + 手势抽屉」
4. **风格统一**：暖棕 + 琥珀配色，中文字面，全页面统一字号/间距体系

### 1.3 非目标（明确不做）

- 不改动 `wifi_manager` / `storage_manager` / `comm` 等非 UI 模块的行为
- 不引入真·拖拽跟手（见 [§10 风险与取舍](#10-风险与取舍)）
- 不改动设备侧通信协议
- 不引入矢量宠物组件（本次保留 ASCII 字符画）

---

## 2. 设计决策

三项已确认的决策，及其影响：

| 决策 | 选择 | 影响 |
|------|------|------|
| **配色** | **Warm 暖棕 + 琥珀** | 替换现有薄荷青；全 UI 颜色常量重定义 |
| **界面语言** | **中文** | 需引入中文字体流水线（见 [§7](#7-中文字体工程)）；所有字面量改为中文 |
| **宠物形象** | **保留 ASCII 字符画** | `buddy_anim.c` 的 18 物种 × 7 状态全部保留，仅调整字体与布局 |

> **ASCII 路线的已知上限**：字符画观感取决于等宽字体渲染质量，不会达到矢量绘制的可爱度。若后续要升级，布局无需改动，只需将 `buddy_anim` 的渲染后端替换为 `sprite_pet`（精灵图）或新增矢量组件。

---

## 3. 当前 UI 现状

### 3.1 结构

```
main/ui/buddy/buddy_ui.c  (1067 行)
├── 颜色/尺寸/布局常量
├── NORMAL 页面   动画区 170px + HUD 面板 + 时钟/WiFi/电池
├── PET 页面      ASCII 预览框 + 底部手势提示   ← 用量条已删
├── INFO 页面     7 页纯文本（About/Buttons/Session/Device/Network/Battery/Credits）
├── 菜单覆盖层    8 项（Settings/WiFi/Petdex/Shutdown/Help/About/Demo/Close）
├── 设置覆盖层    11 项（Brightness…Back）
└── 滑动回调      info_swipe_cb（仅响应左右，用于 Info 翻页 / Petdex 切换）
```

### 3.2 现有配色

```c
#define COLOR_BG       0x08141E   /* 深蓝黑 */
#define COLOR_PANEL    0x0F2A38
#define COLOR_HOT      0x00D4C8   /* 薄荷青 */
#define COLOR_TEXT     0xFFFFFF
#define COLOR_TEXT_DIM 0x6E92A8
```

### 3.3 现有交互

| 输入 | 行为 |
|------|------|
| BOOT 短按 | 无覆盖层时循环切换 NORMAL→PET→INFO；有覆盖层时选中下一项/执行 |
| BOOT 长按 | 无覆盖层时开菜单；有覆盖层时返回/执行 |
| 触摸点击 | 菜单/设置项点击执行 |
| 触摸滑动 | 左右：Info 翻页 / Petdex 切换；**上下方向被丢弃** |

### 3.4 现有问题点（代码层面）

- `buddy_ui.c` 单文件 1067 行，承担页面 + 覆盖层 + 手势 + 状态同步四类职责
- `lvgl_chat_ui.c`（906 行）为遗留代码，**未列入 `main/CMakeLists.txt`**，不参与构建
- 字号只有 14/16 两档，无法表达层级
- 布局常量 `STATUS_AREA_H` / `STATUS_AREA_Y` / `BAR_H` 在用量条删除后部分失效

---

## 4. 目标信息架构

### 4.1 导航模型

**宠物常驻 + 手势抽屉**，取代原「3 模式循环」：

```
                    ┌─────────────────────────────┐
                    │        主界面（宠物）        │
                    │  状态栏 / 角色 / 状态词 / 心情 │
                    └─────────────────────────────┘
                       │                      │
          下滑 ↓       │                      │      上滑 ↑
                       ▼                      ▼
        ┌──────────────────────┐   ┌──────────────────────┐
        │     信息面板（抽屉）   │   │     菜单（抽屉）      │
        │ 日期/电量/WiFi/存储/   │   │ 宠物图鉴/网络/设置/   │
        │ 运行时长/固件          │   │ 关于/关机            │
        └──────────────────────┘   └──────────────────────┘
                                              │
                              ┌───────────────┼───────────────┐
                              ▼               ▼               ▼
                        宠物图鉴页       网络页         设置页
```

### 4.2 输入映射（改造后）

| 输入 | 主界面 | 抽屉/子页面 |
|------|--------|-------------|
| 触摸下滑 | 打开信息面板 | — |
| 触摸上滑 | 打开菜单 | 关闭抽屉 |
| 触摸点击宠物 | 互动（心情/叫声） | — |
| 触摸点击列表项 | — | 执行 |
| BOOT 短按 | 互动 | 选中下一项 / 执行 |
| BOOT 长按 | 打开菜单 | 返回上一层 |
| 左右滑动 | — | 宠物图鉴切换状态 / 网络页翻页 |

### 4.3 状态机

新增抽屉状态（`BuddyUIState` 扩展）：

```c
typedef enum {
    DRAWER_NONE = 0,   /* 主界面 */
    DRAWER_INFO,       /* 下滑信息面板 */
    DRAWER_MENU,       /* 上滑菜单 */
    DRAWER_COUNT
} DrawerState;
```

抽屉打开时主界面下移/变暗，抽屉从底部滑入（约 200ms，`lv_anim`）。

---

## 5. 页面设计

### 5.1 主界面

| 区域 | 内容 | 规格 |
|------|------|------|
| 状态栏 | 时钟 / WiFi 图标 / 电量百分比 | 高 24px，底部 1px 分隔线 |
| 角色区 | ASCII 字符画（居中） | 垂直居中于 y≈110–190 |
| 状态词 | 待机 / 工作中 / 睡眠中 / 完成 / 需要确认 | 角色下方 y≈202，10px 粗体 |
| 心情点 | 3 个圆点，当前档位高亮 | y≈276，间距 12px |
| 手势提示 | 「上滑菜单  下滑信息」 | y≈292，6.2px，弱化色 |

状态词与 Persona 状态映射：

| PersonaState | 中文状态词 |
|---|---|
| `PERSONA_SLEEP` | 睡眠中 |
| `PERSONA_IDLE` | 待机 |
| `PERSONA_BUSY` | 工作中… |
| `PERSONA_ATTENTION` | 需要确认 |
| `PERSONA_CELEBRATE` | 完成！ |
| `PERSONA_DIZZY` | 晕了 |
| `PERSONA_HEART` | 开心 |

### 5.2 信息面板（新增，替代 7 个 Info 页）

下滑抽屉，圆角 11px 顶部，带拖拽把手。内容：

| 行 | 数据来源 |
|---|---|
| 日期 + 时间（标题区） | `buddy_get_local_time()` |
| 电量 | `battery_get_percentage()` / `battery_is_charging()` |
| 网络 | `wifi_manager_get_signal_level()` + `wifi_manager_get_connected_ssid()` |
| 存储 | `storage_get_info(&total, &used)` |
| 运行时长 | `esp_timer_get_time()` 自启动累计 |
| 固件 | 版本字符串（当前为 `buddy_ui.c` 中硬编码 `1.0.0`，本次提取为宏） |

> **信息取舍**：原 Info 页的 About / Buttons / Device / Credits 内容**不再逐页展示**——About/Credits 属一次性信息，迁移到菜单「关于」项；Buttons 帮助文本迁到菜单「帮助」；Device 信息合并进本面板。

### 5.3 菜单（抽屉）

8 项精简为 5 项：

| 序号 | 菜单项 | 动作 |
|---|---|---|
| 0 | 宠物图鉴 | 进入 Petdex 页面 |
| 1 | 网络 | 进入 WiFi 管理页 |
| 2 | 设置 | 打开设置覆盖层 |
| 3 | 关于 | 显示版本/设备信息/按键说明（长按进入 Demo 演示模式） |
| 4 | 关机 | `esp_deep_sleep_start()` |

> **移除项**：`Help` / `Demo` / `Close`。
> - `Help`：按键/手势说明文本并入「关于」页
> - `Demo`：演示模式入口移入「关于」页（长按「关于」项触发），不再占用菜单项
> - `Close`：由「下滑」或「长按 BOOT」返回替代

选中项：`PANEL2` 背景 + 左侧 3px 琥珀竖条 + 粗体。

### 5.4 设置（重做）

分组显示，行高从 28px 降到 19px 以容纳分组标题：

| 分组 | 项 |
|---|---|
| **显示** | 亮度 / 信息面板 / 宠物模式 / 旋转 |
| **连接** | Wi-Fi / 声音 |
| **系统** | 自动休眠 / 恢复出厂 |

> **移除项**：`LED`（无对应硬件）、`Pet`（物种切换已由「宠物图鉴」承担）、`HUD`（改为「信息面板」开关）。`Reset` 更名为「恢复出厂」。

### 5.5 宠物图鉴页（Petdex，重做）

- 顶部：标题「宠物图鉴」+ 右侧「水豚 3/18」物种计数
- 中部：大预览框（圆角 8px，`PANEL` 背景）显示当前物种 ASCII
- 底部：「上一个」/「下一个」按钮 + 「左右滑动切换」提示
- 左右滑动切换物种（复用现有 `buddy_anim_next/prev_species`）

### 5.6 网络页（WiFi UI，重做）

- 顶部：标题「网络」+ 右侧连接状态
- 已连接卡片：SSID / 信号强度 / 已保存标记
- 可用网络列表：每行 SSID + 信号档位（强/中/弱），当前网络高亮
- 保持现有 `wifi_manager` API 调用不变，仅重做视觉

### 5.7 删除项

| 目标 | 原因 |
|---|---|
| `main/ui/lvgl_chat_ui.c` / `.h`（906 行） | 遗留代码，未列入 `CMakeLists.txt`，不参与构建 |
| `create_pet_page` 的 ASCII 预览框 → 迁移 | 由「宠物图鉴」页承接 |
| `INFO_PAGE_*` 七页枚举 | 由信息面板 + 关于页替代 |

---

## 6. 视觉规范

### 6.1 配色（Warm）

```c
#define COLOR_BG       lv_color_hex(0x16110D)  /* 暖黑底 */
#define COLOR_PANEL    lv_color_hex(0x2A211A)  /* 卡片 */
#define COLOR_PANEL2   lv_color_hex(0x372A1F)  /* 选中/高亮 */
#define COLOR_ACCENT   lv_color_hex(0xFFB454)  /* 琥珀主色 */
#define COLOR_TEXT     lv_color_hex(0xFFF6EA)  /* 主文字 */
#define COLOR_TEXT_DIM lv_color_hex(0x9C7F63)  /* 次要文字 */
#define COLOR_LINE     lv_color_hex(0x3E3023)  /* 分隔线 */
#define COLOR_WARM     lv_color_hex(0xFF8A7A)  /* 脸红/警示 */
```

> 保留 `COLOR_GREEN` / `COLOR_RED` 用于状态语义（如充电/错误），但不再用于审批按钮。

### 6.2 字号层级

| 用途 | 字号 | 说明 |
|------|------|------|
| 状态栏时钟 | 9px | 粗体 |
| 状态栏辅助 | 7.5px | 常规 |
| 正文 / 列表项 | 7.6px | 常规 |
| 列表项选中 | 9.5px | 粗体 |
| 页面标题 | 10.5px | 粗体 |
| 大标题（信息面板日期） | 10.5–11px | 粗体 |
| 手势提示 | 6.2px | 弱化色 |

> **重要**：LVGL 的 `lv_font_t` 是固定像素尺寸的点阵字体，不支持任意缩放。上表的字号需通过**生成对应像素高度的字体**实现，而非运行时缩放。实际实现时收敛为 3 档：**12px / 14px / 20px**（见 [§7](#7-中文字体工程)），页面内通过布局与字重区分层级。

### 6.3 间距规范

| 项 | 值 |
|---|---|
| 页面左右边距 | 13px |
| 卡片圆角 | 8–11px |
| 列表行高 | 19–24px |
| 状态栏高度 | 24px |
| 分隔线 | 1px，`COLOR_LINE` |

---

## 7. 中文字体工程

### 7.1 现状问题

`tools/generate_font.py` **只生成空字形表的 C 模板**，脚本自身注释即写明"生成的文件只是模板，需要使用 LVGL 字体转换工具填充实际的字形数据"。当前工程**没有任何可用的中文字体**。

### 7.2 方案

使用 `lv_font_conv`（Node 包，`npx lv_font_conv`，已验证本机可用）从系统 CJK 字体生成点阵字体。

| 项 | 决定 |
|---|---|
| 转换工具 | `lv_font_conv`（npx 调用，无需全局安装） |
| 源字体 | macOS 系统 CJK（`Hiragino Sans GB` / `STHeiti Medium`）；ASCII 字符画用 `Menlo` |
| 字符集 | **按需提取**：脚本扫描源码中的中文字面量 + ASCII 可打印字符 |
| 字号档位 | 12px（正文）/ 14px（小标题）/ 20px（大数字、时钟） |
| bpp | 1（无抗锯齿，体积优先） |
| 压缩 | 不压缩（`--no-compress`，省运行时开销） |
| 预估体积 | **< 40KB**（按需取字；GB2312 全集 16px 约 300KB+，故必须按需） |

### 7.3 流水线

`tools/generate_font.py` 重写为：

1. 扫描 `main/**/*.c`、`main/**/*.h` 提取全部 UTF-8 中文字面量
2. 合并 ASCII 可打印字符（`0x20-0x7E`）
3. 对每个字号档位调用 `lv_font_conv --format lvgl --bpp 1 --font <CJK> --symbols <charset> --lv-font-name lv_font_sc_NN`
4. 输出 `main/font/lv_font_sc_12.c` / `_14.c` / `_20.c`

新增 `main/font/` 目录，并在 `main/CMakeLists.txt` 的 `SRCS` 中加入三个 `.c`。

### 7.4 lv_conf 与 CMake 变更

- `components/lv_conf.h`：`LV_FONT_DEFAULT` 改为 14px 中文字体；保留 Montserrat 作为 fallback
- `main/CMakeLists.txt`：加入字体源文件

### 7.5 已知限制

| 限制 | 说明 | 缓解 |
|---|---|---|
| 动态中文不可显示 | WiFi SSID 等运行期中文不在字符集中，会显示方块 | 若需要，扩大字符集或对 SSID 做 ASCII 降级显示 |
| 改文案需重跑脚本 | 新增中文字面量后必须重新生成字体 | 在 `AGENTS.md` 中记录该流程 |
| 体积随文案增长 | 字符集越大体积越大 | 按需取字，控制在 < 40KB |

---

## 8. 组件拆分与文件改动

### 8.1 拆分策略

`buddy_ui.c` 现 1067 行、职责四类，重做时拆为两个文件：

| 文件 | 职责 |
|---|---|
| `main/ui/buddy/buddy_ui.c` | 主界面、宠物图鉴页、菜单、设置、关于 |
| `main/ui/buddy/buddy_drawer.c`（新增） | 信息面板 + 抽屉状态机 + 滑入手势处理 |

### 8.2 改动清单

| 文件 | 改动类型 | 说明 |
|---|---|---|
| `main/font/lv_font_sc_{12,14,20}.c` | **新增** | 生成的字体 |
| `tools/generate_font.py` | **重写** | 真实转换流水线 |
| `components/lv_conf.h` | 修改 | 注册中文字体，默认字体改 14px |
| `main/CMakeLists.txt` | 修改 | 加入字体源文件与 `buddy_drawer.c` |
| `main/ui/buddy/buddy_ui.c` | **重写** | 配色/布局/中文化/页面重做 |
| `main/ui/buddy/buddy_ui.h` | 修改 | 新增 `DrawerState`；移除 `INFO_PAGE_*`；新增面板 API |
| `main/ui/buddy/buddy_drawer.c` | **新增** | 信息面板与抽屉 |
| `main/ui/buddy/buddy_anim.c` | 微调 | 等宽字体、居中基线 |
| `main/main.c` | 修改 | 抽屉状态机、菜单/设置动作、手势分发 |
| `main/ui/pet_ui.c` | 修改 | 配色与字体统一 |
| `main/ui/wifi_ui.c` | 修改 | 配色与字体统一 |
| `main/ui/lvgl_chat_ui.c` / `.h` | **删除** | 遗留未链接代码 |
| `docs/` + `AGENTS.md` | 修改 | 同步 UI 结构与字体流程 |

### 8.3 接口草案

```c
/* buddy_ui.h 新增 */
typedef enum { DRAWER_NONE = 0, DRAWER_INFO, DRAWER_MENU, DRAWER_COUNT } DrawerState;

void buddy_ui_drawer_open(DrawerState which);
void buddy_ui_drawer_close(void);
DrawerState buddy_ui_drawer_state(void);
bool buddy_ui_drawer_is_open(void);

/* 手势入口（由 main.c 注册到 lvgl_touch） */
void buddy_ui_handle_swipe(lv_dir_t dir);
```

---

## 9. 实施阶段

分 4 个阶段，每阶段结束运行 `idf.py build` 并确认后再进入下一阶段。

### 阶段 1 — 字体流水线（可独立验证）

1. 重写 `tools/generate_font.py`
2. 生成 `main/font/lv_font_sc_{12,14,20}.c`
3. 更新 `lv_conf.h` / `CMakeLists.txt`
4. 在主界面临时显示一句中文验证渲染
5. **验证**：构建通过；字体体积 < 40KB；中文无方块

### 阶段 2 — 主界面 + 视觉规范

1. 替换配色常量（Warm）
2. 重做主界面布局（状态栏 / 角色 / 状态词 / 心情点）
3. 全 UI 中文化
4. `buddy_anim.c` 字体微调
5. **验证**：主界面观感与效果图一致；7 种状态词正确

### 阶段 3 — 抽屉与信息面板

1. 新增 `buddy_drawer.c`（信息面板 + 状态机）
2. 改 `main.c` 手势分发与 BOOT 键逻辑
3. 菜单 8 项 → 5 项
4. **验证**：下滑出面板、上滑出菜单、长按返回

### 阶段 4 — 子页面与清理

1. 重做宠物图鉴页、网络页
2. 重做设置（分组）
3. 删除 `lvgl_chat_ui.c/.h`
4. 更新文档与 `AGENTS.md`
5. **验证**：全页面走查

---

## 10. 风险与取舍

### 10.1 手势跟手性（明确取舍）

**现状**：`main/input/lvgl_touch.c` 只在**松手时**按净位移判定方向并回调，且上下方向被上层丢弃。

**本次方案**：保留「松手判定」，新增上下方向处理 + 抽屉滑入动画。**不做真·拖拽跟手**。

**理由**：跟手拖拽需改造 `lvgl_touch.c` 输出连续坐标并接入 LVGL 手势系统，属输入驱动改动，风险与工作量显著更高。

**后续可选**：若手感不足，再单独评估跟手方案。

### 10.2 其他风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 字体体积 | Flash 占用增加 | 按需取字，目标 < 40KB；构建后核对 |
| `-Werror` 截断告警 | UTF-8 中文字面量长度计算易触发 `format-truncation` | 缓冲留足；用 `sizeof(buf)-1` |
| `buddy_ui.c` 重写回归 | 菜单/设置动作可能遗漏 | 保持 `main.c` 的 `execute_*_action` 回调契约不变 |
| 中文字体缺字 | 运行期动态中文显示方块 | 见 [§7.5](#75-已知限制) |
| NVS 旧数据 | 上版删除的 `appr`/`deny` 等键残留 | 无影响，可忽略；「恢复出厂」可清 |

---

## 11. 验证方案

| 项 | 方法 |
|---|---|
| 构建 | `idf.py build`，要求 0 error / 0 warning |
| 字体体积 | 构建后核对 `main/font/*.c` 合计 < 40KB |
| 中文渲染 | 逐页走查，确认无缺字方块 |
| 手势 | 下滑出面板、上滑出菜单、长按返回、左右切换图鉴 |
| BOOT 键 | 短按选中/互动、长按返回/开菜单 |
| 页面 | 主界面 7 状态、信息面板、菜单、设置、宠物图鉴、网络页 |
| 回归 | Wi-Fi 扫描/连接、Petdex 动画切换、自动休眠、亮度调节 |
| 结构 | `python3 tools/verify_structure.py` |

---

## 附录 A: 效果图

完整页面效果图（170×320，2× 出图，含 Warm 配色与中文）：

```
/Users/yulinling/.codex/visualizations/2026/10/11/01a1298e-a8ae-72f3-a984-de8291d23b03/ui-final.png
```

图中内容（4 列 × 3 行）：

| 位置 | 内容 |
|---|---|
| 第 1 行 | 主界面：待机 / 工作中 / 睡眠中 / 完成 |
| 第 2 行 | 下滑信息面板 / 上滑菜单 / 设置 / （审批卡片已移除） |
| 第 3 行 | 宠物图鉴 / 网络页 / 主界面（需要确认）/ 主界面（完成） |

> 说明：图中审批卡片为定稿前的设计稿，**审批功能已于 commit `7a69393` 移除**，实施时不含该项。

---

## 附录 B: 已完成的相关改动

本次 UI 重做**之前**已完成、本设计不再包含的内容：

| 提交 | 内容 |
|---|---|
| `7afcfb1` | 移除 opencode/codex/pi/claude/vscode 对接，固件去品牌化 |
| `7a69393` | 移除审批链路与套餐用量，心情改为随机 |

因此本设计中的「设置」「菜单」不再包含审批相关项，`buddy_stats_mood_tier()` 已为随机实现。
