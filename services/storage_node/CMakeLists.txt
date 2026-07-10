nimbus_add_proto_library(nimbus_storage_node_proto PROTOS ${CMAKE_SOURCE_DIR}/proto/storage_node/storage_node.proto)

add_library(nimbus_storage_node STATIC
    src/chunk_id.cpp
    src/local_disk_chunk_store.cpp
    src/fault_injecting_chunk_store.cpp
    src/capability_token.cpp
    src/storage_node_service_impl.cpp
)
target_include_directories(nimbus_storage_node PUBLIC include)
target_link_libraries(nimbus_storage_node PUBLIC nimbus_common nimbus_storage_node_proto)
nimbus_apply_compiler_settings(nimbus_storage_node)

add_executable(storage_node src/main.cpp)
target_include_directories(storage_node PUBLIC include)
target_link_libraries(storage_node PRIVATE nimbus_storage_node nimbus_common nimbus_storage_node_proto)
nimbus_apply_compiler_settings(storage_node)

if(NIMBUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
