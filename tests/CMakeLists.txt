add_executable(batchguard_tests
    integration/concurrent_hashing_test.cpp
    integration/progress_reporting_test.cpp
    integration/robustness_test.cpp
    integration/scanner_test.cpp
    stage1/smoke_test.cpp
    unit/cli_app_test.cpp
    unit/console_progress_test.cpp
    unit/content_fingerprint_test.cpp
    unit/duplicate_group_test.cpp
    unit/file_hash_scheduler_test.cpp
    unit/file_discovery_test.cpp
    unit/input_validator_test.cpp
)

target_include_directories(batchguard_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/core"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(batchguard_tests
    PRIVATE
        batchguard_cli_support
        batchguard_core
        GTest::gtest_main
)

set_target_properties(batchguard_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(batchguard_tests PRIVATE /utf-8)
endif()

include(GoogleTest)
gtest_discover_tests(batchguard_tests)
