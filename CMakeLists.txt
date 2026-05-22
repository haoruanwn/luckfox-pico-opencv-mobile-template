cmake_minimum_required(VERSION 3.10)
project(rv1106-vision-runtime)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(RMG_BUILD_DEVICE_TARGETS "Build RK MPI-backed runtime library and examples" ON)
option(RMG_BUILD_EXAMPLES "Build example programs" ON)
option(RMG_BUILD_TESTS "Build runtime tests" OFF)

# 当编译选项为Debug时，启用调试符号
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

# ========================================
# 全局编译选项
# ========================================
# spdlog 编译时日志级别控制
# 设置此选项可在编译时完全移除低于该级别的日志代码（零开销）
# 可选值:
#   0 = TRACE (显示所有日志)
#   1 = DEBUG
#   2 = INFO (默认，推荐用于发布版本)
#   3 = WARN
#   4 = ERROR
#   5 = CRITICAL
#   6 = OFF (完全禁用所有日志)
set(SPDLOG_ACTIVE_LEVEL "2" CACHE STRING "Compile-time log level (0=TRACE, 6=OFF)")

# ========================================
# 第三方库路径
# ========================================
set(LUCKFOX_MPI_PATH ${CMAKE_SOURCE_DIR}/3rdparty/luckfox_pico_rkmpi_example)
set(SPDLOG_PATH ${CMAKE_SOURCE_DIR}/3rdparty/spdlog)

# ========================================
# MPI 库列表（取自 luckfox-pico-rkmpi-example）
# 当前 reset 只保留相机采集底座；后续恢复 VENC/RTSP 时再补充对应库。
# ========================================
set(MPI_LIBS
    sample_comm
    rockit
    rockchip_mpp
    rkaiq
    rga
    pthread
    dl
)

# ========================================
# spdlog 配置
# ========================================
find_package(Threads REQUIRED)
add_subdirectory(${SPDLOG_PATH} spdlog)

# ========================================
# 添加子目录
# ========================================
if(RMG_BUILD_DEVICE_TARGETS)
    # 源码
    add_subdirectory(src)

    if(RMG_BUILD_EXAMPLES)
        # 最小示例程序
        add_subdirectory(examples/camera_capture)
        add_subdirectory(examples/camera_runtime_smoke)
    endif()
endif()

if(RMG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/runtime_tests)
endif()

message(STATUS "spdlog compile-time log level: ${SPDLOG_ACTIVE_LEVEL} (0=TRACE, 2=INFO, 6=OFF)")
