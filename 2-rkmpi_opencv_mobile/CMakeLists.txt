cmake_minimum_required(VERSION 3.15)

project(rkmpi-opencv-mobile-test)

set(CMAKE_CXX_STANDARD 20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(OpenCV_DIR "../opencv-mobile-4.12.0-luckfox-pico/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME} main.cpp luckfox_mpi.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
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

target_link_directories(${PROJECT_NAME} PRIVATE 
    ${OpenCV_LIB_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})