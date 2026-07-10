#pragma once
#include <memory>
#include <mutex>
#include <optional>
#include <chrono>
#include "nimbus/storage_node/i_chunk_store.h"

namespace nimbus::storage_node {

class FaultInjectingChunkStore final : public IChunkStore {
 public:
  explicit FaultInjectingChunkStore(std::unique_ptr<IChunkStore> delegate);

  void InjectFailureOnNextCall(ChunkStoreError error_to_inject);
  void InjectFailureAfterNCalls(size_t n, ChunkStoreError error_to_inject);
  void InjectLatency(std::chrono::milliseconds latency);
  void ClearInjectedFaults();

  common::Result<std::monostate, ChunkStoreErrorDetail> Put(const ChunkId& id, std::span<const std::byte> data) override;
  common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> Get(const ChunkId& id) override;
  common::Result<std::monostate, ChunkStoreErrorDetail> VerifyOnly(const ChunkId& id) override;
  common::Result<std::monostate, ChunkStoreErrorDetail> Delete(const ChunkId& id) override;
  bool Exists(const ChunkId& id) override;
  ChunkStoreStats GetStats() override;
  std::vector<ChunkId> ListAllChunkIds() override;

 private:
  bool ShouldInjectFault(ChunkStoreError& out_error);
  void ApplyLatency();

  std::unique_ptr<IChunkStore> delegate_;
  std::mutex fault_state_mutex_;
  
  std::optional<ChunkStoreError> pending_one_shot_fault_;
  std::optional<std::pair<size_t, ChunkStoreError>> pending_nth_call_fault_;
  size_t call_counter_{0};
  std::chrono::milliseconds pending_latency_{0};
};

}  // namespace nimbus::storage_node
