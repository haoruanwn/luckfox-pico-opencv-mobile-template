project(opencv-mobile-test)
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)

set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/opencv-mobile-4.12.0-luckfox-pico/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(opencv-mobile-test main.cpp)

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})