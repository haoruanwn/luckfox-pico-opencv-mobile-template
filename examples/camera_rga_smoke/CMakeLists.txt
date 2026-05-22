set(TARGET_NAME CameraRgaSmoke)

add_executable(${TARGET_NAME} main.cpp)

target_compile_definitions(${TARGET_NAME} PRIVATE
    SPDLOG_ACTIVE_LEVEL=${SPDLOG_ACTIVE_LEVEL}
)

target_link_libraries(${TARGET_NAME}
    rmg
    ${MPI_LIBS}
    stdc++fs
)
