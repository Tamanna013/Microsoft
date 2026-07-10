#include <gtest/gtest.h>
#include "nimbus/storage_node/fault_injecting_chunk_store.h"
#include "nimbus/common/sha256.h"

using namespace nimbus::storage_node;

class MockChunkStore : public IChunkStore {
public:
    common::Result<std::monostate, ChunkStoreErrorDetail> Put(const ChunkId&, std::span<const std::byte>) override {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
    }
    common::Result<std::vector<std::byte>, ChunkStoreErrorDetail> Get(const ChunkId&) override {
        return common::Result<std::vector<std::byte>, ChunkStoreErrorDetail>::Ok({});
    }
    common::Result<std::monostate, ChunkStoreErrorDetail> VerifyOnly(const ChunkId&) override {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
    }
    common::Result<std::monostate, ChunkStoreErrorDetail> Delete(const ChunkId&) override {
        return common::Result<std::monostate, ChunkStoreErrorDetail>::Ok({});
    }
    bool Exists(const ChunkId&) override { return true; }
    ChunkStoreStats GetStats() override { return {1, 100, 1000}; }
    std::vector<ChunkId> ListAllChunkIds() override { return {}; }
};

TEST(FaultInjectingChunkStoreTest, InjectOnNextCall) {
    FaultInjectingChunkStore store(std::make_unique<MockChunkStore>());
    auto id = ChunkId::FromDigest(nimbus::common::Sha256::HashBytes({}));

    store.InjectFailureOnNextCall(ChunkStoreError::IoError);
    auto res = store.Put(id, {});
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error().kind, ChunkStoreError::IoError);

    // Should work on next call
    auto res2 = store.Put(id, {});
    EXPECT_TRUE(res2.IsOk());
}

TEST(FaultInjectingChunkStoreTest, InjectAfterNCalls) {
    FaultInjectingChunkStore store(std::make_unique<MockChunkStore>());
    auto id = ChunkId::FromDigest(nimbus::common::Sha256::HashBytes({}));

    store.InjectFailureAfterNCalls(2, ChunkStoreError::IoError);
    EXPECT_TRUE(store.Put(id, {}).IsOk()); // call 1
    EXPECT_TRUE(store.Put(id, {}).IsOk()); // call 2
    auto res = store.Put(id, {}); // call 3 -> fail
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error().kind, ChunkStoreError::IoError);
}

TEST(FaultInjectingChunkStoreTest, ClearFaults) {
    FaultInjectingChunkStore store(std::make_unique<MockChunkStore>());
    auto id = ChunkId::FromDigest(nimbus::common::Sha256::HashBytes({}));

    store.InjectFailureOnNextCall(ChunkStoreError::IoError);
    store.ClearInjectedFaults();
    auto res = store.Put(id, {});
    EXPECT_TRUE(res.IsOk());
}
