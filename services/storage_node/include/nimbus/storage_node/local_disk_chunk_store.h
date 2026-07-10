#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <array>
#include <atomic>
#include "nimbus/storage_node/i_chunk_store.h"
#include "nimbus/common/logger.h"

namespace nimbus::storage_node {

/**
 * @class LocalDiskChunkStore
 * @brief Production implementation of IChunkStore, utilizing sharded layout and striped locking.
 */
class LocalDiskChunkStore final : public IChunkStore {
 public:
  LocalDiskChunkStore(std::filesystem::path data_root,
                       std::shared_ptr<const nimbus::common::Logger> logger);

  common::Result<std::monostate, ChunkStoreErrorDetail> Put(const ChunkId& id, std::span<const std::byte> data) override;
  common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> Get(const ChunkId& id) override;
  common::Result<std::monostate, ChunkStoreErrorDetail> VerifyOnly(const ChunkId& id) override;
  common::Result<std::monostate, ChunkStoreErrorDetail> Delete(const ChunkId& id) override;
  bool Exists(const ChunkId& id) override;
  ChunkStoreStats GetStats() override;
  std::vector<ChunkId> ListAllChunkIds() override;

 private:
  std::filesystem::path ShardDirFor(const ChunkId& id) const;
  std::filesystem::path FinalPathFor(const ChunkId& id) const;

  std::filesystem::path data_root_;
  std::shared_ptr<const nimbus::common::Logger> logger_;

  static constexpr size_t kLockStripeCount = 256;
  mutable std::array<std::mutex, kLockStripeCount> stripe_locks_;
  std::mutex& LockFor(const ChunkId& id) const;

  std::atomic<uint64_t> chunk_count_{0};
  std::atomic<uint64_t> bytes_used_{0};
};

}  // namespace nimbus::storage_node
