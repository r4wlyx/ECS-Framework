cmake_minimum_required(VERSION 3.10.0)
project(ECS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(app ${SOURCES})

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(app PRIVATE
    $<$<CONFIG:Release>:
        -O3
        -ffast-math
        -march=native
    >
    $<$<CONFIG:RelWithDebInfo>:
        -O2
        -g
    >
)