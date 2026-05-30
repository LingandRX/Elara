#!/bin/bash

# ESP-IDF 项目构建脚本
# 使用方法: ./build.sh [command]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查 ESP-IDF 环境
check_idf_env() {
    if [ -z "$IDF_PATH" ]; then
        print_error "ESP-IDF 环境未设置"
        print_info "请运行: . $HOME/esp/esp-idf/export.sh"
        exit 1
    fi
    print_info "ESP-IDF 路径: $IDF_PATH"
}

# 设置目标芯片
set_target() {
    print_info "设置目标芯片为 ESP32-S3..."
    idf.py set-target esp32s3
}

# 编译项目
build() {
    print_info "编译项目..."
    idf.py build
    print_info "编译完成"
}

# 烧录固件
flash() {
    local port=${1:-/dev/tty.usbmodem*}
    print_info "烧录固件到 $port..."
    idf.py -p "$port" flash
    print_info "烧录完成"
}

# 监控串口
monitor() {
    local port=${1:-/dev/tty.usbmodem*}
    print_info "监控串口 $port..."
    idf.py -p "$port" monitor
}

# 烧录并监控
flash_monitor() {
    local port=${1:-/dev/tty.usbmodem*}
    print_info "烧录并监控 $port..."
    idf.py -p "$port" flash monitor
}

# 清理构建产物
clean() {
    print_info "清理构建产物..."
    idf.py fullclean
    print_info "清理完成"
}

# 菜单配置
menuconfig() {
    print_info "打开菜单配置..."
    idf.py menuconfig
}

# 显示帮助
show_help() {
    echo "ESP-IDF 项目构建脚本"
    echo ""
    echo "使用方法: $0 [command]"
    echo ""
    echo "命令:"
    echo "  set-target    设置目标芯片为 ESP32-S3"
    echo "  build         编译项目"
    echo "  flash [port]  烧录固件 (默认端口: /dev/tty.usbmodem*)"
    echo "  monitor [port] 监控串口"
    echo "  flash-monitor [port] 烧录并监控"
    echo "  clean         清理构建产物"
    echo "  menuconfig    打开菜单配置"
    echo "  help          显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0 build"
    echo "  $0 flash /dev/tty.usbmodem0"
    echo "  $0 flash-monitor"
}

# 主函数
main() {
    local command=${1:-help}

    check_idf_env

    case $command in
        set-target)
            set_target
            ;;
        build)
            build
            ;;
        flash)
            flash "$2"
            ;;
        monitor)
            monitor "$2"
            ;;
        flash-monitor)
            flash_monitor "$2"
            ;;
        clean)
            clean
            ;;
        menuconfig)
            menuconfig
            ;;
        help|*)
            show_help
            ;;
    esac
}

main "$@"
