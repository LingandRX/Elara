# 测试目录 CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# 包含 ESP-IDF 测试框架
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 注册测试组件
idf_component_register(
    SRCS
        "test_lvgl.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        lvgl
        unity
)
