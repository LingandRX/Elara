#!/usr/bin/env python3
"""中文字体流水线：源码提取字符集 -> lv_font_conv -> LVGL C 源文件。"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MAIN_DIR = REPO / "main"
FONT_DIR = MAIN_DIR / "font"

# ASCII/符号优先用等宽字体，保证 buddy_anim 的字符画对齐。
# lv_font_conv 不支持 macOS 字体集合（TTC），因此使用对应的单体系统字体。
MONO_FONT = "/System/Library/Fonts/Monaco.ttf"
CJK_FONT = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"

STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')

# 预留常用字，减少改文案就重跑字体的频率。
# 为满足三档字体源码合计小于 60KB 的硬性预算，预留集保持精简。
# 预留常用字，减少改文案就重跑字体的频率；当前以源码字面量为主，控制字体体积。
EXTRA_CHARS = ()


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


def compact_source(path: Path) -> None:
    """Remove converter comments and compact numeric literals without changing C."""
    text = path.read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"0x([0-9A-Fa-f]+)", lambda m: str(int(m.group(1), 16)), text)
    match = re.search(r"(static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap\[\] = )\{(.*?)\};", text, re.S)
    if match:
        bitmap = bytes(int(value) for value in re.findall(r"\d+", match.group(2)))
        encoded = "".join(f"\\x{byte:02x}" for byte in bitmap)
        text = text[:match.start()] + match.group(1) + f'"{encoded}";' + text[match.end():]
    text = re.sub(r",[ \t]+", ",", text)
    text = re.sub(r"\{[ \t]+", "{", text)
    text = re.sub(r"[ \t]+\}", "}", text)
    text = re.sub(r"\n[ \t]*\n+", "\n", text)
    path.write_text(text, encoding="utf-8")


def build_font(size: int, charset: str, out_dir: Path) -> Path:
    cjk_charset = "".join(ch for ch in charset if ord(ch) > 0x7F)
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"lv_font_sc_{size}.c"
    cmd = [
        "npx", "--yes", "lv_font_conv",
        "--size", str(size),
        "--bpp", "1",
        "--no-compress",
        "--no-kerning",
        "--format", "lvgl",
        "--lv-include", "lvgl.h",
        "--lv-font-name", f"lv_font_sc_{size}",
        "--font", MONO_FONT, "--range", "0x20-0x7E",
        "--font", CJK_FONT, "--symbols", cjk_charset,
        "-o", str(out),
    ]
    subprocess.run(cmd, check=True, cwd=REPO)
    compact_source(out)
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
