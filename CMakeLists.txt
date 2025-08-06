cmake_minimum_required(VERSION 3.15)

project(luckfox_pico_rtsp_opencv)

set(CMAKE_CXX_STANDARD 11)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(LIBC_TYPE "uclibc")

add_definitions(-DRV1106_1103)

# --- 库查找 (保留) ---
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/${LIBC_TYPE}/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} main.cc)

add_compile_options(-g -Wall -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF -DROCKIVA -DUAPI2 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rknn
    ${CMAKE_CURRENT_SOURCE_DIR}/include/librga
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/uAPI2
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/common
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/xcore
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/algos
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser_v2
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/smartIr
)

target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS} 
    rknnmrt 
    Threads::Threads 
    sample_comm 
    rockit 
    rockchip_mpp 
    rkaiq 
    pthread 
    rtsp 
    stdc++ 
    func_none 
    c 
    m 
    rt 
    dl 
    rga
)