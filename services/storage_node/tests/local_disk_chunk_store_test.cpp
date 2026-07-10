#include <gtest/gtest.h>
#include "nimbus/storage_node/local_disk_chunk_store.h"
#include "nimbus/common/sha256.h"
#include <fstream>
#include <thread>
#include <vector>

using namespace nimbus::storage_node;
using namespace nimbus::common;

class LocalDiskChunkStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / ("nimbus_test_" + GenerateUuidV4());
        logger_ = std::make_shared<Logger>("logs/test_op.log", "logs/test_audit.log");
        store_ = std::make_unique<LocalDiskChunkStore>(temp_dir_, logger_);
    }

    void TearDown() override {
        if (std::filesystem::exists(temp_dir_)) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    std::filesystem::path temp_dir_;
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<LocalDiskChunkStore> store_;
};

TEST_F(LocalDiskChunkStoreTest, PutGetSuccess) {
    std::vector<std::byte> data = {std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(data));

    EXPECT_TRUE(store_->Put(chunk_id, data).IsOk());
    EXPECT_TRUE(store_->Exists(chunk_id));

    auto get_res = store_->Get(chunk_id);
    ASSERT_TRUE(get_res.IsOk());
    EXPECT_EQ(get_res.Value().size(), data.size());
    EXPECT_EQ(get_res.Value()[0], data[0]);

    EXPECT_TRUE(store_->VerifyOnly(chunk_id).IsOk());
}

TEST_F(LocalDiskChunkStoreTest, PutChecksumMismatch) {
    std::vector<std::byte> data = {std::byte{'A'}};
    std::vector<std::byte> wrong_data = {std::byte{'B'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(wrong_data));

    auto res = store_->Put(chunk_id, data);
    ASSERT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error().kind, ChunkStoreError::ChecksumMismatch);
    EXPECT_FALSE(store_->Exists(chunk_id));
}

TEST_F(LocalDiskChunkStoreTest, CorruptionDetection) {
    std::vector<std::byte> data = {std::byte{'X'}, std::byte{'Y'}, std::byte{'Z'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(data));

    EXPECT_TRUE(store_->Put(chunk_id, data).IsOk());

    // Corrupt the file manually
    std::string hex = chunk_id.ToHexString();
    std::filesystem::path final_path = temp_dir_ / hex.substr(0, 2) / hex.substr(2, 2) / (hex + ".chunk");
    
    std::ofstream ofs(final_path, std::ios::binary | std::ios::out);
    ofs.write("BAD", 3);
    ofs.close();

    auto get_res = store_->Get(chunk_id);
    ASSERT_FALSE(get_res.IsOk());
    EXPECT_EQ(get_res.Error().kind, ChunkStoreError::CorruptionDetected);

    auto verify_res = store_->VerifyOnly(chunk_id);
    ASSERT_FALSE(verify_res.IsOk());
    EXPECT_EQ(verify_res.Error().kind, ChunkStoreError::CorruptionDetected);
}

TEST_F(LocalDiskChunkStoreTest, IdempotentDelete) {
    std::vector<std::byte> data = {std::byte{'D'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(data));

    EXPECT_TRUE(store_->Put(chunk_id, data).IsOk());

    EXPECT_TRUE(store_->Delete(chunk_id).IsOk());
    EXPECT_FALSE(store_->Exists(chunk_id));

    // Delete again should succeed (idempotent)
    EXPECT_TRUE(store_->Delete(chunk_id).IsOk());
}

TEST_F(LocalDiskChunkStoreTest, CrashConsistencyStrayTempFilesIgnored) {
    std::vector<std::byte> data = {std::byte{'T'}, std::byte{'M'}, std::byte{'P'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(data));
    std::string hex = chunk_id.ToHexString();
    
    std::filesystem::path shard_dir = temp_dir_ / hex.substr(0, 2) / hex.substr(2, 2);
    std::filesystem::create_directories(shard_dir);
    std::filesystem::path stray_temp = shard_dir / (hex + ".tmp." + GenerateUuidV4());
    
    std::ofstream ofs(stray_temp, std::ios::binary);
    ofs.write("TMP", 3);
    ofs.close();

    EXPECT_FALSE(store_->Exists(chunk_id));
    auto get_res = store_->Get(chunk_id);
    ASSERT_FALSE(get_res.IsOk());
    EXPECT_EQ(get_res.Error().kind, ChunkStoreError::NotFound);
}

TEST_F(LocalDiskChunkStoreTest, StatsAccuracy) {
    std::vector<std::byte> data1 = {std::byte{'1'}};
    std::vector<std::byte> data2 = {std::byte{'2'}, std::byte{'3'}};
    
    auto chunk_id1 = ChunkId::FromDigest(Sha256::HashBytes(data1));
    auto chunk_id2 = ChunkId::FromDigest(Sha256::HashBytes(data2));

    EXPECT_TRUE(store_->Put(chunk_id1, data1).IsOk());
    EXPECT_TRUE(store_->Put(chunk_id2, data2).IsOk());

    auto stats = store_->GetStats();
    EXPECT_EQ(stats.chunk_count, 2);
    EXPECT_EQ(stats.bytes_used, 3); // 1 + 2

    EXPECT_TRUE(store_->Delete(chunk_id1).IsOk());
    stats = store_->GetStats();
    EXPECT_EQ(stats.chunk_count, 1);
    EXPECT_EQ(stats.bytes_used, 2);
}

TEST_F(LocalDiskChunkStoreTest, ConcurrentDeduplicationRace) {
    std::vector<std::byte> data = {std::byte{'R'}, std::byte{'A'}, std::byte{'C'}, std::byte{'E'}};
    auto chunk_id = ChunkId::FromDigest(Sha256::HashBytes(data));

    auto writer = [&]() {
        for (int i = 0; i < 100; ++i) {
            EXPECT_TRUE(store_->Put(chunk_id, data).IsOk());
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(writer);
    }
    for (auto& t : threads) {
        t.join();
    }

    auto stats = store_->GetStats();
    EXPECT_EQ(stats.chunk_count, 1);
    EXPECT_EQ(stats.bytes_used, 4);
    EXPECT_TRUE(store_->VerifyOnly(chunk_id).IsOk());
}
