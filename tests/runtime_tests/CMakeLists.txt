add_executable(runtime_tests runtime_tests.cpp)

target_include_directories(runtime_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(runtime_tests PRIVATE Threads::Threads)
target_compile_features(runtime_tests PRIVATE cxx_std_17)

add_test(NAME runtime_tests COMMAND runtime_tests)
