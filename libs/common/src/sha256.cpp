#include "nimbus/common/sha256.h"
#include <openssl/evp.h>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace nimbus::common {

struct Sha256::Impl {
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx;
  Impl() : ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
    if (!ctx) throw std::runtime_error("Failed to create EVP_MD_CTX");
    if (1 != EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr)) {
      throw std::runtime_error("Failed to init SHA256");
    }
  }
};

Sha256::Sha256() : impl_(std::make_unique<Impl>()) {}
Sha256::~Sha256() = default;
Sha256::Sha256(Sha256&&) noexcept = default;
Sha256& Sha256::operator=(Sha256&&) noexcept = default;

void Sha256::Update(std::span<const std::byte> data) {
  if (finalized_) throw std::logic_error("Called Update after Finalize");
  if (data.empty()) return;
  if (1 != EVP_DigestUpdate(impl_->ctx.get(), data.data(), data.size())) {
    throw std::runtime_error("Failed to update SHA256");
  }
}

std::array<std::byte, 32> Sha256::Finalize() {
  if (finalized_) throw std::logic_error("Called Finalize twice");
  finalized_ = true;
  std::array<std::byte, 32> digest;
  unsigned int len = 0;
  if (1 != EVP_DigestFinal_ex(impl_->ctx.get(), reinterpret_cast<unsigned char*>(digest.data()), &len)) {
    throw std::runtime_error("Failed to finalize SHA256");
  }
  return digest;
}

std::array<std::byte, 32> Sha256::HashBytes(std::span<const std::byte> data) {
  Sha256 hasher;
  hasher.Update(data);
  return hasher.Finalize();
}

std::string Sha256::ToHexString(const std::array<std::byte, 32>& digest) {
  std::ostringstream oss;
  for (auto b : digest) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
  }
  return oss.str();
}

Result<std::array<std::byte, 32>, Sha256::HashError> Sha256::FromHexString(std::string_view hex) {
  if (hex.size() != 64) return Result<std::array<std::byte, 32>, HashError>::Err(HashError::InvalidHexLength);
  std::array<std::byte, 32> digest;
  for (size_t i = 0; i < 32; ++i) {
    std::string byte_str(hex.substr(i * 2, 2));
    size_t pos = 0;
    try {
      int b = std::stoi(byte_str, &pos, 16);
      if (pos != 2) return Result<std::array<std::byte, 32>, HashError>::Err(HashError::InvalidHexCharacter);
      digest[i] = static_cast<std::byte>(b);
    } catch (...) {
      return Result<std::array<std::byte, 32>, HashError>::Err(HashError::InvalidHexCharacter);
    }
  }
  return Result<std::array<std::byte, 32>, HashError>::Ok(digest);
}

}  // namespace nimbus::common
