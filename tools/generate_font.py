#!/usr/bin/env python3
"""
LVGL 中文字体生成脚本
使用方法: python3 generate_font.py
"""

import os
import sys
import requests
from pathlib import Path

# 常用中文字符集 (GB2312 一级汉字)
COMMON_CHINESE = """
的一是不了人我在有他这为之大来以个中上们到说国和地也子时道出会三要于下得可你年生
自学对所家用当天过小作理公多日方如已经把与那由此种长好向表市万老位成最新明月前行
从文中年起发定开两东同长本分主被又无手看前走面体正开之最及部样子现进动她两用还
此正使点从样正文力所心灵理公多日方如已经把与那由此种长好向表市万老位成最新明月
前行从文中年起发定开两东同长本分主被又无手看前走面体正开之最及部样子现进动她两
用还此正使点从样正文力所心灵
"""

# ASCII 可打印字符 (0x20-0x7E)
ASCII_CHARS = ''.join(chr(i) for i in range(0x20, 0x7F))

def generate_font_c_file(output_path, font_size=16):
    """
    生成 LVGL 字体 C 文件

    参数:
        output_path: 输出文件路径
        font_size: 字体大小 (像素)
    """

    # 合并字符集
    all_chars = ASCII_CHARS + COMMON_CHINESE.strip()

    # 去重
    unique_chars = sorted(set(all_chars))

    print(f"字符数量: {len(unique_chars)}")
    print(f"字体大小: {font_size}px")
    print(f"输出文件: {output_path}")

    # 生成 C 代码
    c_code = f'''/**
 * @file lv_font_chinese_{font_size}.c
 * 中文字体实现 - {font_size}px
 * 自动生成，请勿手动编辑
 */

#include "lv_font_chinese_{font_size}.h"

/* 字体描述符 */
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
    /* 这里需要添加字形描述 */
}};

/* 字体映射 */
static const uint8_t font_bitmap[] = {{
    /* 这里需要添加字形数据 */
}};

/* 字体范围 */
static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = 0x20,
        .range_length = 95,
        .glyph_id_start = 0,
        .unicode_list = NULL,
        .glyph_id_ofs_list = NULL,
        .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    }},
}};

/* 字体格式描述 */
static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = font_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .cmap_num = 1,
    .bpp = 1,
    .kern_scale = 0,
    .kern_dsc = NULL,
    .kern_classes = 0,
}};

/* 字体对象 */
const lv_font_t lv_font_chinese_{font_size} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {font_size},
    .base_line = 0,
    .subpx = LV_FONT_SUBPX_NONE,
    .dsc = &font_dsc,
    .fallback = NULL,
    .user_data = NULL,
}};

/**
 * 获取中文字体 ({font_size}px)
 */
const lv_font_t *lv_font_chinese_{font_size}_get(void) {{
    return &lv_font_chinese_{font_size};
}}
'''

    # 写入文件
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(c_code)

    print(f"已生成: {output_path}")

def main():
    """主函数"""

    # 获取项目根目录
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    # 输出目录
    output_dir = project_root / "main" / "font"
    output_dir.mkdir(parents=True, exist_ok=True)

    # 生成字体文件
    output_path = output_dir / "lv_font_chinese_16.c"
    generate_font_c_file(output_path, font_size=16)

    print("\n注意: 生成的文件只是模板，需要使用 LVGL 字体转换工具填充实际的字形数据。")
    print("字体转换工具: https://lvgl.io/tools/fontconverter")

if __name__ == "__main__":
    main()
