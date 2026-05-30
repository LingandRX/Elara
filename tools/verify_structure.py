#!/usr/bin/env python3
"""
项目结构验证脚本
检查必要的文件和目录是否存在
"""

import os
import sys
from pathlib import Path

# 必要的文件列表
REQUIRED_FILES = [
    "CMakeLists.txt",
    "main/CMakeLists.txt",
    "main/main.c",
    "main/display/sh8601.c",
    "main/display/sh8601.h",
    "main/display/lvgl_display.c",
    "main/display/lvgl_display.h",
    "main/input/lvgl_touch.c",
    "main/input/lvgl_touch.h",
    "main/ui/lvgl_chat_ui.c",
    "main/ui/lvgl_chat_ui.h",
    "main/ui/lvgl_widgets.c",
    "main/ui/lvgl_widgets.h",
    "main/comm/uart_comm.c",
    "main/comm/uart_comm.h",
    "components/lvgl/idf_component.yml",
    "components/i2c_bsp/i2c_bsp.c",
    "components/i2c_bsp/i2c_bsp.h",
    "components/esp_touch/touch_bsp.c",
    "components/esp_touch/touch_bsp.h",
    "docs/ESP32-S3-Touch-LCD-1.9.md",
    "docs/LVGL_Integration.md",
    "docs/Project_Summary.md",
    "examples/lvgl_example.c",
    "tools/generate_font.py",
    "build.sh",
    "Makefile",
    "README.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "LICENSE",
    ".clang-format",
    ".editorconfig",
    ".gitignore",
    "sdkconfig.defaults",
    "AGENTS.md",
]

# 必要的目录列表
REQUIRED_DIRS = [
    "main",
    "main/display",
    "main/input",
    "main/ui",
    "main/font",
    "main/comm",
    "components",
    "components/lvgl",
    "components/i2c_bsp",
    "components/esp_touch",
    "docs",
    "examples",
    "tools",
    "test",
    ".github",
    ".github/workflows",
    ".github/ISSUE_TEMPLATE",
]

def check_files(project_root):
    """检查文件是否存在"""
    missing_files = []
    for file_path in REQUIRED_FILES:
        full_path = project_root / file_path
        if not full_path.exists():
            missing_files.append(file_path)
    return missing_files

def check_dirs(project_root):
    """检查目录是否存在"""
    missing_dirs = []
    for dir_path in REQUIRED_DIRS:
        full_path = project_root / dir_path
        if not full_path.is_dir():
            missing_dirs.append(dir_path)
    return missing_dirs

def main():
    """主函数"""
    # 获取项目根目录
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    print(f"项目根目录: {project_root}")
    print("")

    # 检查文件
    print("检查文件...")
    missing_files = check_files(project_root)
    if missing_files:
        print(f"❌ 缺少 {len(missing_files)} 个文件:")
        for file in missing_files:
            print(f"   - {file}")
    else:
        print("✅ 所有必要的文件都存在")

    print("")

    # 检查目录
    print("检查目录...")
    missing_dirs = check_dirs(project_root)
    if missing_dirs:
        print(f"❌ 缺少 {len(missing_dirs)} 个目录:")
        for dir in missing_dirs:
            print(f"   - {dir}")
    else:
        print("✅ 所有必要的目录都存在")

    print("")

    # 总结
    if missing_files or missing_dirs:
        print("❌ 项目结构验证失败")
        return 1
    else:
        print("✅ 项目结构验证通过")
        return 0

if __name__ == "__main__":
    sys.exit(main())
