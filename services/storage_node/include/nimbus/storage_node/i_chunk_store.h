#pragma once
#include <span>
#include <vector>
#include <cstdint>
#include <string>
#include <variant>
#include "nimbus/common/result.h"
#include "nimbus/storage_node/chunk_id.h"

namespace nimbus::storage_node {

enum class ChunkStoreError {
  ChecksumMismatch,      // Put: claimed id doesn't match actual content hash
  CorruptionDetected,    // Get/VerifyOnly: on-disk bytes don't match filename-derived id
  NotFound,
  IoError,               // underlying filesystem operation failed
};

struct ChunkStoreErrorDetail {
  ChunkStoreError kind;
  std::string message;
};

struct ChunkStoreStats {
  uint64_t chunk_count;
  uint64_t bytes_used;
  uint64_t bytes_free;
};

/**
 * @class IChunkStore
 * @brief Fault-injectable seam and abstraction over chunk storage engine.
 */
class IChunkStore {
 public:
  virtual ~IChunkStore() = default;

  virtual common::Result<std::monostate, ChunkStoreErrorDetail> Put(
      const ChunkId& id, std::span<const std::byte> data) = 0;

  virtual common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> Get(
      const ChunkId& id) = 0;

  virtual common::Result<std::monostate, ChunkStoreErrorDetail> VerifyOnly(
      const ChunkId& id) = 0;

  virtual common::Result<std::monostate, ChunkStoreErrorDetail> Delete(
      const ChunkId& id) = 0;

  virtual bool Exists(const ChunkId& id) = 0;

  virtual ChunkStoreStats GetStats() = 0;

  virtual std::vector<ChunkId> ListAllChunkIds() = 0;
};

}  // namespace nimbus::storage_node
