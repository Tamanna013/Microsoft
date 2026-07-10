#include "nimbus/common/protocol_version.h"

namespace nimbus::common {

Result<std::monostate, ProtocolVersionError> CheckProtocolVersion(
    uint32_t received_version, uint32_t min_supported, uint32_t max_supported) {
  if (received_version < min_supported || received_version > max_supported) {
    return Result<std::monostate, ProtocolVersionError>::Err(ProtocolVersionError::Unsupported);
  }
  return Result<std::monostate, ProtocolVersionError>::Ok({});
}

}  // namespace nimbus::common
