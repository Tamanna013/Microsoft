#pragma once
#include <cstdint>

namespace nimbus::storage_node {

// Fixed wire chunk-frame size for streamed requests and responses.
constexpr uint64_t kStreamFrameSizeBytes = 256 * 1024; // 256 KiB

}  // namespace nimbus::storage_node
