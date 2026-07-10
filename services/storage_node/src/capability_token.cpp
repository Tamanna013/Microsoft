#include "nimbus/storage_node/capability_token.h"

namespace nimbus::storage_node {

common::Result<std::monostate, CapabilityError> ValidateCapabilityToken(
    std::string_view token, const ChunkId& scoped_chunk_id) {
    // STUB(M17): always returns Ok.
    return common::Result<std::monostate, CapabilityError>::Ok({});
}

}  // namespace nimbus::storage_node
