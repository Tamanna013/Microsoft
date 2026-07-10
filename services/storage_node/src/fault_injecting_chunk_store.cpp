#include "nimbus/storage_node/fault_injecting_chunk_store.h"
#include <thread>

namespace nimbus::storage_node {

FaultInjectingChunkStore::FaultInjectingChunkStore(std::unique_ptr<IChunkStore> delegate)
    : delegate_(std::move(delegate)) {}

void FaultInjectingChunkStore::InjectFailureOnNextCall(ChunkStoreError error_to_inject) {
    std::lock_guard<std::mutex> lock(fault_state_mutex_);
    pending_one_shot_fault_ = error_to_inject;
}

void FaultInjectingChunkStore::InjectFailureAfterNCalls(size_t n, ChunkStoreError error_to_inject) {
    std::lock_guard<std::mutex> lock(fault_state_mutex_);
    pending_nth_call_fault_ = {n, error_to_inject};
    call_counter_ = 0;
}

void FaultInjectingChunkStore::InjectLatency(std::chrono::milliseconds latency) {
    std::lock_guard<std::mutex> lock(fault_state_mutex_);
    pending_latency_ = latency;
}

void FaultInjectingChunkStore::ClearInjectedFaults() {
    std::lock_guard<std::mutex> lock(fault_state_mutex_);
    pending_one_shot_fault_.reset();
    pending_nth_call_fault_.reset();
    call_counter_ = 0;
    pending_latency_ = std::chrono::milliseconds::zero();
}

bool FaultInjectingChunkStore::ShouldInjectFault(ChunkStoreError& out_error) {
    std::lock_guard<std::mutex> lock(fault_state_mutex_);
    call_counter_++;

    if (pending_one_shot_fault_.has_value()) {
        out_error = *pending_one_shot_fault_;
        pending_one_shot_fault_.reset();
        return true;
    }

    if (pending_nth_call_fault_.has_value()) {
        if (call_counter_ > pending_nth_call_fault_->first) {
            out_error = pending_nth_call_fault_->second;
            return true;
        }
    }

    return false;
}

void FaultInjectingChunkStore::ApplyLatency() {
    std::chrono::milliseconds latency;
    {
        std::lock_guard<std::mutex> lock(fault_state_mutex_);
        latency = pending_latency_;
    }
    if (latency > std::chrono::milliseconds::zero()) {
        std::this_thread::sleep_for(latency);
    }
}

common::Result<std::monostate, ChunkStoreErrorDetail> FaultInjectingChunkStore::Put(const ChunkId& id, std::span<const std::byte> data) {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err({injected_error, "Injected Fault"});
    }
    return delegate_->Put(id, data);
}

common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> FaultInjectingChunkStore::Get(const ChunkId& id) {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Err({injected_error, "Injected Fault"});
    }
    return delegate_->Get(id);
}

common::Result<std::monostate, ChunkStoreErrorDetail> FaultInjectingChunkStore::VerifyOnly(const ChunkId& id) {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err({injected_error, "Injected Fault"});
    }
    return delegate_->VerifyOnly(id);
}

common::Result<std::monostate, ChunkStoreErrorDetail> FaultInjectingChunkStore::Delete(const ChunkId& id) {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Err({injected_error, "Injected Fault"});
    }
    return delegate_->Delete(id);
}

bool FaultInjectingChunkStore::Exists(const ChunkId& id) {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return false;
    }
    return delegate_->Exists(id);
}

ChunkStoreStats FaultInjectingChunkStore::GetStats() {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return {0, 0, 0};
    }
    return delegate_->GetStats();
}

std::vector<ChunkId> FaultInjectingChunkStore::ListAllChunkIds() {
    ApplyLatency();
    ChunkStoreError injected_error;
    if (ShouldInjectFault(injected_error)) {
        return {};
    }
    return delegate_->ListAllChunkIds();
}

}  // namespace nimbus::storage_node
