#pragma once
#include <cstdint>
#include <variant>
#include "nimbus/common/result.h"

namespace nimbus::common {

constexpr uint32_t kCurrentProtocolVersion = 1;

enum class ProtocolVersionError { Unsupported };

/**
 * @brief Checks if a received protocol version falls within the supported range.
 * 
 * Every future gRPC service method implementation MUST call this as its first statement.
 * Mapped to grpc::Status with FAILED_PRECONDITION on error.
 */
[[nodiscard]] Result<std::monostate, ProtocolVersionError> CheckProtocolVersion(
    uint32_t received_version, uint32_t min_supported, uint32_t max_supported);

}  // namespace nimbus::common
