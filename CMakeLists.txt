cmake_minimum_required(VERSION 3.20)
project(NimbusFS VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include shared compiler settings (warnings, clang-tidy, etc.)
include(cmake/NimbusCompilerSettings.cmake)
include(cmake/NimbusProto.cmake)

option(NIMBUS_BUILD_TESTS "Build test suite" ON)

if(NIMBUS_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests/integration)
endif()

# Services
add_subdirectory(libs/common)
add_subdirectory(libs/nimbus_client)
add_subdirectory(services/metadata_service)
add_subdirectory(services/coordinator)
add_subdirectory(services/storage_node)
add_subdirectory(apps/nimbus_cli)
add_subdirectory(apps/nimbus_syncd)

if(NIMBUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
