cmake_minimum_required(VERSION 4.2)
project(BatchGuard LANGUAGES CXX)

add_library(batchguard_core STATIC
    src/core/batchguard_core.cpp
)

target_include_directories(batchguard_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(batchguard_cli
    apps/cli/main.cpp
)

target_link_libraries(batchguard_cli
    PRIVATE
        batchguard_core
)

set_target_properties(
    batchguard_core
    batchguard_cli
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

set_target_properties(batchguard_cli
    PROPERTIES
        OUTPUT_NAME BatchGuard
)
