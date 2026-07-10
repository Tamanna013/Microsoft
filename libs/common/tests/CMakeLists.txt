add_executable(nimbus_common_tests 
    logger_test.cpp 
    config_test.cpp 
    result_test.cpp
    sha256_test.cpp
    clock_test.cpp
    lease_test.cpp
    path_validator_test.cpp
    bounded_blocking_queue_test.cpp
    protocol_version_test.cpp
    uuid_test.cpp
)
target_link_libraries(nimbus_common_tests PRIVATE GTest::gtest_main nimbus_common)
nimbus_apply_compiler_settings(nimbus_common_tests)
include(GoogleTest)
gtest_discover_tests(nimbus_common_tests)
