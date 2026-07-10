find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(nimbus_common STATIC 
    src/common_stub.cpp
    src/logger.cpp
    src/config.cpp
    src/config_schema.cpp
    src/sha256.cpp
    src/clock.cpp
    src/lease.cpp
    src/path_validator.cpp
    src/protocol_version.cpp
    src/uuid.cpp
)
target_include_directories(nimbus_common PUBLIC include)
target_link_libraries(nimbus_common PUBLIC spdlog::spdlog nlohmann_json::nlohmann_json fmt::fmt OpenSSL::Crypto)
nimbus_apply_compiler_settings(nimbus_common)


# Version header generation
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nimbus/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/nimbus/version.h
    @ONLY
)
target_include_directories(nimbus_common PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
