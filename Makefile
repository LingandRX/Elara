# Elara 项目 Makefile
# 简化常用开发操作

.PHONY: help build flash monitor clean menuconfig set-target

# 默认端口
PORT ?= /dev/tty.usbmodem*

# 帮助信息
help:
	@echo "Elara 项目构建命令"
	@echo ""
	@echo "使用方法:"
	@echo "  make build          编译项目"
	@echo "  make flash          烧录固件"
	@echo "  make monitor        监控串口"
	@echo "  make flash-monitor  烧录并监控"
	@echo "  make clean          清理构建产物"
	@echo "  make menuconfig     打开菜单配置"
	@echo "  make set-target     设置目标芯片"
	@echo ""
	@echo "变量:"
	@echo "  PORT=/dev/tty.usbmodem0  指定串口"
	@echo ""
	@echo "示例:"
	@echo "  make build"
	@echo "  make flash PORT=/dev/tty.usbmodem0"

# 设置目标芯片
set-target:
	idf.py set-target esp32s3

# 编译项目
build:
	idf.py build

# 烧录固件
flash:
	idf.py -p $(PORT) flash

# 监控串口
monitor:
	idf.py -p $(PORT) monitor

# 烧录并监控
flash-monitor:
	idf.py -p $(PORT) flash monitor

# 清理构建产物
clean:
	idf.py fullclean

# 打开菜单配置
menuconfig:
	idf.py menuconfig

# 格式化代码
format:
	find main -name "*.c" -o -name "*.h" | xargs clang-format -i

# 检查代码风格
lint:
	find main -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror

# 生成字体
font:
	python3 tools/generate_font.py

# 显示项目信息
info:
	@echo "项目名称: Elara"
	@echo "目标芯片: ESP32-S3"
	@echo "开发框架: ESP-IDF v6.0.1"
	@echo "图形库: LVGL v9.1"
	@echo "显示驱动: ST7789V2"
	@echo "触摸芯片: CST816T"

# 运行测试
test:
	@echo "运行 LVGL 集成测试..."
	@echo "注意: 测试需要在目标设备上运行"

# 检查代码
check: lint
	@echo "代码检查完成"

# 验证项目结构
verify:
	python3 tools/verify_structure.py

# 项目统计
stats:
	python3 tools/project_stats.py
