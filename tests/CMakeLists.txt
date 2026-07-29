add_executable(batchguard_tests
    stage1/smoke_test.cpp
)

target_link_libraries(batchguard_tests
    PRIVATE
        batchguard_core
        GTest::gtest_main
)

set_target_properties(batchguard_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

include(GoogleTest)
gtest_discover_tests(batchguard_tests)
