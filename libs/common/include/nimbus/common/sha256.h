#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <span>
#include "nimbus/common/result.h"

namespace nimbus::common {

/**
 * @class Sha256
 * @brief Streaming SHA-256 hash wrapper over OpenSSL's EVP API.
 * 
 * Thread-safety: Not thread-safe. A single hash computation is inherently sequential.
 * Concurrent independent computations should instantiate their own `Sha256` instances.
 * 
 * Memory ownership: Internal OpenSSL context is fully RAII-managed via Pimpl.
 */
class Sha256 {
 public:
  Sha256();
  ~Sha256();
  Sha256(const Sha256&) = delete;
  Sha256& operator=(const Sha256&) = delete;
  Sha256(Sha256&&) noexcept;
  Sha256& operator=(Sha256&&) noexcept;

  void Update(std::span<const std::byte> data);
  [[nodiscard]] std::array<std::byte, 32> Finalize();

  static std::array<std::byte, 32> HashBytes(std::span<const std::byte> data);
  static std::string ToHexString(const std::array<std::byte, 32>& digest);

  enum class HashError { InvalidHexLength, InvalidHexCharacter };
  static Result<std::array<std::byte, 32>, HashError> FromHexString(std::string_view hex);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool finalized_ = false;
};

}  // namespace nimbus::common
