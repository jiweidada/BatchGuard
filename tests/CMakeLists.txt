add_executable(batchguard_tests
    stage1/smoke_test.cpp
    unit/cli_app_test.cpp
    unit/input_validator_test.cpp
)

target_include_directories(batchguard_tests
    PRIVATE
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
