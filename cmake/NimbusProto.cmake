find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

function(nimbus_add_proto_library target_name)
    cmake_parse_arguments(ARG "" "" "PROTOS" ${ARGN})
    if(NOT ARG_PROTOS)
        message(FATAL_ERROR "nimbus_add_proto_library called without PROTOS")
    endif()

    set(OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/nimbus/proto")
    file(MAKE_DIRECTORY ${OUT_DIR})

    set(IMPORT_DIRS)
    foreach(proto ${ARG_PROTOS})
        get_filename_component(abs_proto ${proto} ABSOLUTE)
        get_filename_component(dir ${abs_proto} DIRECTORY)
        list(APPEND IMPORT_DIRS ${dir})
    endforeach()
    if(IMPORT_DIRS)
        list(REMOVE_DUPLICATES IMPORT_DIRS)
    endif()

    protobuf_generate(
        LANGUAGE cpp
        OUT_VAR PB_SRCS
        PROTOC_OUT_DIR ${OUT_DIR}
        IMPORT_DIRS ${IMPORT_DIRS}
        PROTOS ${ARG_PROTOS}
    )

    protobuf_generate(
        LANGUAGE grpc
        PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        OUT_VAR GRPC_SRCS
        PROTOC_OUT_DIR ${OUT_DIR}
        IMPORT_DIRS ${IMPORT_DIRS}
        PROTOS ${ARG_PROTOS}
    )

    add_library(${target_name} STATIC ${PB_SRCS} ${GRPC_SRCS})
    
    # Expose the parent of nimbus/proto so `#include "nimbus/proto/..."` works
    target_include_directories(${target_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    
    target_link_libraries(${target_name} PUBLIC protobuf::libprotobuf gRPC::grpc++)
    nimbus_apply_compiler_settings(${target_name})
endfunction()
