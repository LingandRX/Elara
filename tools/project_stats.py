#!/usr/bin/env python3
"""
项目统计脚本
统计代码行数、文件数量等
"""

import os
import sys
from pathlib import Path
from collections import defaultdict

# 文件类型统计
FILE_TYPES = {
    ".c": "C 源文件",
    ".h": "C 头文件",
    ".py": "Python 脚本",
    ".md": "Markdown 文档",
    ".txt": "文本文件",
    ".yml": "YAML 配置",
    ".sh": "Shell 脚本",
}

def count_lines(file_path):
    """统计文件行数"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            return sum(1 for _ in f)
    except:
        return 0

def scan_directory(project_root):
    """扫描目录"""
    stats = {
        "files": defaultdict(int),
        "lines": defaultdict(int),
        "total_files": 0,
        "total_lines": 0,
    }

    # 忽略的目录
    ignore_dirs = {
        "build",
        "cmake-build-debug",
        "cmake-build-debug-1",
        "cmake-build-debug-esp-idf",
        "managed_components",
        ".git",
        ".idea",
        "__pycache__",
    }

    for root, dirs, files in os.walk(project_root):
        # 过滤忽略的目录
        dirs[:] = [d for d in dirs if d not in ignore_dirs]

        for file in files:
            file_path = Path(root) / file
            suffix = file_path.suffix.lower()

            # 统计文件类型
            if suffix in FILE_TYPES:
                stats["files"][suffix] += 1
                stats["total_files"] += 1

                # 统计行数
                lines = count_lines(file_path)
                stats["lines"][suffix] += lines
                stats["total_lines"] += lines

    return stats

def print_stats(stats):
    """打印统计信息"""
    print("📊 项目统计")
    print("=" * 50)

    print("\n📁 文件类型统计:")
    print("-" * 50)
    for suffix, count in sorted(stats["files"].items()):
        type_name = FILE_TYPES.get(suffix, "其他")
        lines = stats["lines"][suffix]
        print(f"  {suffix:8s} {type_name:12s} {count:4d} 个文件, {lines:6d} 行")

    print("-" * 50)
    print(f"  {'总计':8s} {'':12s} {stats['total_files']:4d} 个文件, {stats['total_lines']:6d} 行")

    print("\n📈 代码行数分布:")
    print("-" * 50)

    # 计算百分比
    if stats["total_lines"] > 0:
        for suffix, lines in sorted(stats["lines"].items()):
            percentage = (lines / stats["total_lines"]) * 100
            bar = "█" * int(percentage / 2)
            print(f"  {suffix:8s} {bar:25s} {percentage:5.1f}%")

def main():
    """主函数"""
    # 获取项目根目录
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    print(f"项目根目录: {project_root}")
    print("")

    # 扫描目录
    stats = scan_directory(project_root)

    # 打印统计
    print_stats(stats)

    return 0

if __name__ == "__main__":
    sys.exit(main())
