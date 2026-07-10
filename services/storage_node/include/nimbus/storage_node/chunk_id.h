#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include "nimbus/common/result.h"

namespace nimbus::storage_node {

/**
 * @class ChunkId
 * @brief A validated wrapper around a 64-character lowercase hex SHA-256 digest string.
 */
class ChunkId {
 public:
  enum class ParseError { WrongLength, InvalidHexCharacter, NotLowercase };

  static common::Result<ChunkId, ParseError> Parse(std::string_view hex_string);
  static ChunkId FromDigest(const std::array<std::byte, 32>& digest);

  [[nodiscard]] const std::string& ToHexString() const;
  [[nodiscard]] const std::array<std::byte, 32>& RawDigest() const;

  bool operator==(const ChunkId&) const = default;

  struct Hash {
    size_t operator()(const ChunkId& id) const;
  };

 private:
  explicit ChunkId(std::array<std::byte, 32> digest, std::string hex);
  
  std::array<std::byte, 32> digest_;
  std::string hex_;
};

}  // namespace nimbus::storage_node
