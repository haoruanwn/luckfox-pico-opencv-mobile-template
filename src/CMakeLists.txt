# ========================================
# RV1106 vision runtime 静态库
# ========================================

set(RMG_SOURCES
    Frame.cpp
    nodes/CameraNode.cpp
    nodes/RgaNode.cpp
    sys/IspDevice.cpp
    sys/MpiSystem.cpp
    sys/ViChannel.cpp
)

set(RMG_HEADERS
    rmg.hpp
    Frame.hpp
    nodes/CameraNode.hpp
    nodes/RgaNode.hpp
    runtime/BoundedQueue.hpp
    runtime/Cancellation.hpp
    runtime/Error.hpp
    runtime/Node.hpp
    runtime/NodeState.hpp
    runtime/NodeStats.hpp
    runtime/Result.hpp
    runtime/WorkerThread.hpp
    sys/IspDevice.hpp
    sys/MpiSystem.hpp
    sys/ViChannel.hpp
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
target_link_libraries(rmg PUBLIC spdlog::spdlog Threads::Threads)

# C++17 特性
target_compile_features(rmg PUBLIC cxx_std_17)

# 设置编译时日志级别
target_compile_definitions(rmg PRIVATE 
    SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL}
)
