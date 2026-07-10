#include "nimbus/storage_node/chunk_id.h"
#include "nimbus/common/sha256.h"
#include <cctype>

namespace nimbus::storage_node {

ChunkId::ChunkId(std::array<std::byte, 32> digest, std::string hex)
    : digest_(digest), hex_(std::move(hex)) {}

common::Result<ChunkId, ChunkId::ParseError> ChunkId::Parse(std::string_view hex_string) {
  if (hex_string.size() != 64) {
    return common::Result<ChunkId, ParseError>::Err(ParseError::WrongLength);
  }
  for (char c : hex_string) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return common::Result<ChunkId, ParseError>::Err(ParseError::InvalidHexCharacter);
    }
    if (std::isupper(static_cast<unsigned char>(c))) {
      return common::Result<ChunkId, ParseError>::Err(ParseError::NotLowercase);
    }
  }
  
  auto digest_res = common::Sha256::FromHexString(hex_string);
  if (!digest_res.IsOk()) {
      return common::Result<ChunkId, ParseError>::Err(ParseError::InvalidHexCharacter);
  }
  return common::Result<ChunkId, ParseError>::Ok(ChunkId(digest_res.Value(), std::string(hex_string)));
}

ChunkId ChunkId::FromDigest(const std::array<std::byte, 32>& digest) {
  return ChunkId(digest, common::Sha256::ToHexString(digest));
}

const std::string& ChunkId::ToHexString() const {
  return hex_;
}

const std::array<std::byte, 32>& ChunkId::RawDigest() const {
  return digest_;
}

size_t ChunkId::Hash::operator()(const ChunkId& id) const {
  size_t hash_val = 0;
  // Combine the first few bytes since SHA256 is already uniformly distributed
  const auto& digest = id.RawDigest();
  for (size_t i = 0; i < sizeof(size_t) && i < digest.size(); ++i) {
      hash_val = (hash_val << 8) | static_cast<size_t>(digest[i]);
  }
  return hash_val;
}

}  // namespace nimbus::storage_node
