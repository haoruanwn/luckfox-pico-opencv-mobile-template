# ========================================
# RV1106 vision runtime 静态库
# ========================================

set(RMG_SOURCES
    MediaFrame.cpp
    SystemManager.cpp
    VideoCapture.cpp
)

set(RMG_HEADERS
    rmg.hpp
    MediaFrame.hpp
    MediaModule.hpp
    SystemManager.hpp
    VideoCapture.hpp
)

# 创建静态库
add_library(rmg STATIC ${RMG_SOURCES} ${RMG_HEADERS})

# 包含路径
target_include_directories(rmg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LUCKFOX_MPI_PATH}/include
    ${LUCKFOX_MPI_PATH}/include/rkaiq
    ${LUCKFOX_MPI_PATH}/include/rkaiq/uAPI2
    ${LUCKFOX_MPI_PATH}/include/rkaiq/common
    ${LUCKFOX_MPI_PATH}/include/rkaiq/xcore
    ${LUCKFOX_MPI_PATH}/include/rkaiq/iq_parser
    ${LUCKFOX_MPI_PATH}/include/rkaiq/iq_parser_v2
    ${LUCKFOX_MPI_PATH}/include/rkaiq/algos
    ${CMAKE_SOURCE_DIR}/3rdparty/luckfox_pico_rkmpi_example/include
)

# 链接目录
target_link_directories(rmg PUBLIC
    ${LUCKFOX_MPI_PATH}/lib/uclibc
    ${CMAKE_SOURCE_DIR}/3rdparty/luckfox_pico_rkmpi_example/lib
)

# 链接 spdlog
target_link_libraries(rmg PUBLIC spdlog::spdlog)

# C++17 特性
target_compile_features(rmg PUBLIC cxx_std_17)

# 设置编译时日志级别
target_compile_definitions(rmg PRIVATE 
    SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL}
)
