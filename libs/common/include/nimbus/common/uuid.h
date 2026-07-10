#pragma once
#include <string>

namespace nimbus::common {

/**
 * @brief Generates a strictly standard-compliant UUIDv4 string.
 */
[[nodiscard]] std::string GenerateUuidV4();

}  // namespace nimbus::common
