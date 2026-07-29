cmake_minimum_required(VERSION 4.2)
project(BatchGuard VERSION 0.1.0 LANGUAGES CXX)

option(BATCHGUARD_BUILD_TESTS "Build BatchGuard tests" ON)

add_library(batchguard_core STATIC
    src/core/input_validator.cpp
)

target_include_directories(batchguard_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(batchguard_cli_support STATIC
    apps/cli/cli_app.cpp
    apps/cli/console_encoding.cpp
)

target_include_directories(batchguard_cli_support
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/cli"
)

target_link_libraries(batchguard_cli_support
    PRIVATE
        batchguard_core
)

target_compile_definitions(batchguard_cli_support
    PRIVATE
        BATCHGUARD_VERSION="${PROJECT_VERSION}"
)

add_executable(batchguard_cli
    apps/cli/main.cpp
)

target_link_libraries(batchguard_cli
    PRIVATE
        batchguard_cli_support
)

set_target_properties(
    batchguard_core
    batchguard_cli_support
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

if(MSVC)
    target_compile_options(batchguard_core PRIVATE /utf-8)
    target_compile_options(batchguard_cli_support PRIVATE /utf-8)
    target_compile_options(batchguard_cli PRIVATE /utf-8)
endif()

if(BATCHGUARD_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Third_Party/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "GoogleTest is missing. Expected Third_Party/googletest/CMakeLists.txt"
        )
    endif()

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(Third_Party/googletest EXCLUDE_FROM_ALL)

    enable_testing()
    add_subdirectory(tests)
endif()
