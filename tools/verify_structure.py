#!/usr/bin/env python3
"""
项目结构验证脚本
检查当前源码树中的关键路径是否存在（以仓库现状为准，非历史 sh8601/build.sh 结构）。
"""

import sys
from pathlib import Path

# 必要的文件列表（与当前工程一致）
REQUIRED_FILES = [
    "CMakeLists.txt",
    "main/CMakeLists.txt",
    "main/main.c",
    "main/idf_component.yml",
    "main/wifi_manager.c",
    "main/wifi_manager.h",
    "main/storage_manager.c",
    "main/storage_manager.h",
    "main/display/lv_port_disp.c",
    "main/display/lv_port_disp.h",
    "main/display/lcd_config.h",
    "main/display/backlight_manager.c",
    "main/display/backlight_manager.h",
    "main/input/lvgl_touch.c",
    "main/input/lvgl_touch.h",
    "main/ui/buddy/buddy_ui.c",
    "main/ui/buddy/buddy_ui.h",
    "main/buddy/buddy_state.c",
    "main/buddy/buddy_state.h",
    "main/comm/uart_comm.c",
    "main/comm/uart_comm.h",
    "main/comm/tcp_server.c",
    "main/comm/tcp_server.h",
    "components/lv_conf.h",
    "components/i2c_bsp/i2c_bsp.c",
    "components/i2c_bsp/i2c_bsp.h",
    "components/esp_touch/touch_bsp.c",
    "components/esp_touch/touch_bsp.h",
    "docs/ESP32-S3-Touch-LCD-1.9.md",
    "docs/LVGL_Integration.md",
    "docs/Project_Summary.md",
    "docs/WiFi_Manager.md",
    "tools/generate_font.py",
    "tools/elara_host.py",
    "Makefile",
    "README.md",
    "LICENSE",
    ".clang-format",
    ".editorconfig",
    ".gitignore",
    "sdkconfig.defaults",
    "partitions.csv",
    "AGENTS.md",
]

REQUIRED_DIRS = [
    "main",
    "main/display",
    "main/input",
    "main/ui",
    "main/ui/buddy",
    "main/buddy",
    "main/battery",
    "main/comm",
    "components",
    "components/i2c_bsp",
    "components/esp_touch",
    "docs",
    "tools",
    "test",
]


def check_files(project_root):
    missing = []
    for file_path in REQUIRED_FILES:
        if not (project_root / file_path).exists():
            missing.append(file_path)
    return missing


def check_dirs(project_root):
    missing = []
    for dir_path in REQUIRED_DIRS:
        if not (project_root / dir_path).is_dir():
            missing.append(dir_path)
    return missing


def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent

    print(f"项目根目录: {project_root}")
    print("")

    print("检查文件...")
    missing_files = check_files(project_root)
    if missing_files:
        print(f"❌ 缺少 {len(missing_files)} 个文件:")
        for f in missing_files:
            print(f"   - {f}")
    else:
        print("✅ 所有必要的文件都存在")

    print("")
    print("检查目录...")
    missing_dirs = check_dirs(project_root)
    if missing_dirs:
        print(f"❌ 缺少 {len(missing_dirs)} 个目录:")
        for d in missing_dirs:
            print(f"   - {d}")
    else:
        print("✅ 所有必要的目录都存在")

    print("")
    if missing_files or missing_dirs:
        print("❌ 项目结构验证失败")
        return 1
    print("✅ 项目结构验证通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
