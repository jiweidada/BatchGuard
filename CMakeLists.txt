cmake_minimum_required(VERSION 4.2)
project(BatchGuard VERSION 0.1.0 LANGUAGES CXX)

option(BATCHGUARD_BUILD_TESTS "Build BatchGuard tests" ON)
option(BATCHGUARD_BUILD_GUI "Build the BatchGuard Qt GUI" OFF)

find_package(Threads REQUIRED)

add_library(batchguard_core STATIC
    src/core/content_fingerprint.cpp
    src/core/duplicate_group.cpp
    src/core/file_discovery.cpp
    src/core/file_hash_scheduler.cpp
    src/core/file_hasher.cpp
    src/core/input_validator.cpp
    src/core/scanner.cpp
)

target_include_directories(batchguard_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(batchguard_core
    PRIVATE
        bcrypt
        Threads::Threads
)

add_library(batchguard_logging STATIC
    src/logging/logging.cpp
)

target_include_directories(batchguard_logging
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(batchguard_cli_support STATIC
    apps/cli/cli_app.cpp
    apps/cli/console_encoding.cpp
    apps/cli/console_progress.cpp
)

target_include_directories(batchguard_cli_support
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/cli"
)

target_link_libraries(batchguard_cli_support
    PUBLIC
        batchguard_core
        batchguard_logging
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
    batchguard_logging
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
    target_compile_options(batchguard_logging PRIVATE /utf-8)
    target_compile_options(batchguard_cli_support PRIVATE /utf-8)
    target_compile_options(batchguard_cli PRIVATE /utf-8)
endif()

if(BATCHGUARD_BUILD_GUI)
    set(batchguardQtComponents Widgets)
    if(BATCHGUARD_BUILD_TESTS)
        list(APPEND batchguardQtComponents Test)
    endif()
    find_package(Qt5 5.14 REQUIRED COMPONENTS ${batchguardQtComponents})

    add_library(batchguard_gui_support STATIC
        apps/gui/duplicate_group_model.cpp
        apps/gui/duplicate_group_model.h
        apps/gui/duplicate_path_model.cpp
        apps/gui/duplicate_path_model.h
        apps/gui/failure_model.cpp
        apps/gui/failure_model.h
        apps/gui/gui_log_model.cpp
        apps/gui/gui_log_model.h
        apps/gui/main_window.cpp
        apps/gui/main_window.h
        apps/gui/main_window.ui
        apps/gui/result_summary.cpp
        apps/gui/result_summary.h
        apps/gui/scan_controller.cpp
        apps/gui/scan_controller.h
        apps/gui/scan_execution.h
        apps/gui/scan_state.h
        apps/gui/threaded_scan_execution.cpp
        apps/gui/threaded_scan_execution.h
    )

    target_include_directories(batchguard_gui_support
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/gui"
    )

    target_link_libraries(batchguard_gui_support
        PUBLIC
            batchguard_core
            batchguard_logging
            Qt5::Widgets
    )

    add_executable(batchguard_gui WIN32
        apps/gui/main.cpp
        apps/gui/terminal_console.cpp
        apps/gui/terminal_console.h
    )

    target_link_libraries(batchguard_gui
        PRIVATE
            batchguard_gui_support
    )

    set_target_properties(
        batchguard_gui_support
        batchguard_gui
        PROPERTIES
            AUTOMOC ON
            AUTOUIC ON
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )

    set_target_properties(batchguard_gui
        PROPERTIES
            OUTPUT_NAME BatchGuardGui
    )

    if(MSVC)
        target_compile_options(batchguard_gui_support PRIVATE /utf-8)
        target_compile_options(batchguard_gui PRIVATE /utf-8)
    endif()

    if(WIN32)
        get_target_property(
            batchguardQtQmakeExecutable
            Qt5::qmake
            IMPORTED_LOCATION
        )
        get_filename_component(
            batchguardQtBinDirectory
            "${batchguardQtQmakeExecutable}"
            DIRECTORY
        )
        find_program(
            BATCHGUARD_WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${batchguardQtBinDirectory}"
            NO_DEFAULT_PATH
            REQUIRED
        )

        # 将本地运行所需的 Qt DLL 和平台插件部署到可执行文件目录。
        add_custom_command(
            TARGET batchguard_gui
            POST_BUILD
            COMMAND
                "${CMAKE_COMMAND}"
                -E
                env
                "PATH=${batchguardQtBinDirectory};$ENV{PATH}"
                "${BATCHGUARD_WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-compiler-runtime
                --verbose 0
                "$<TARGET_FILE:batchguard_gui>"
            COMMENT "Deploying the Qt runtime for BatchGuardGui"
            VERBATIM
        )
    endif()
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
