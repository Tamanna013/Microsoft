#pragma once
#include <variant>
#include <string_view>
#include "nimbus/common/result.h"
#include "nimbus/storage_node/chunk_id.h"

namespace nimbus::storage_node {

enum class CapabilityError { Denied };

// STUB(M17): always returns Ok. Real implementation validates an
// HMAC-signed, chunk_id-bound, nonce-protected, generation-keyed token
// per DRR ARCH-CHG-003. Every call site in this milestone is already
// wired correctly; only this function's body changes in M17.
[[nodiscard]] common::Result<std::monostate, CapabilityError> ValidateCapabilityToken(
    std::string_view token, const ChunkId& scoped_chunk_id);

}  // namespace nimbus::storage_node
