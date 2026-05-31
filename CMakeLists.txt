cmake_minimum_required(VERSION 3.16)

# 强制覆盖分区表配置，无视 sdkconfig
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
set(CONFIG_PARTITION_TABLE_CUSTOM y)
set(CONFIG_PARTITION_TABLE_CUSTOM_FILENAME "partitions.csv")
set(CONFIG_PARTITION_TABLE_FILENAME "partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Elara)
