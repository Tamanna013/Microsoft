add_executable(nimbus_storage_node_tests
    chunk_id_test.cpp
    local_disk_chunk_store_test.cpp
    fault_injecting_chunk_store_test.cpp
    storage_node_service_test.cpp
    replicate_chunk_integration_test.cpp
)
target_link_libraries(nimbus_storage_node_tests PRIVATE GTest::gtest_main nimbus_storage_node nimbus_common nimbus_storage_node_proto)
nimbus_apply_compiler_settings(nimbus_storage_node_tests)

gtest_discover_tests(nimbus_storage_node_tests)
