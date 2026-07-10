#include <gtest/gtest.h>
#include "nimbus/storage_node/chunk_id.h"
#include <unordered_set>

using namespace nimbus::storage_node;

TEST(ChunkIdTest, ParseValid) {
    std::string hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    auto id_res = ChunkId::Parse(hex);
    ASSERT_TRUE(id_res.IsOk());
    EXPECT_EQ(id_res.Value().ToHexString(), hex);
}

TEST(ChunkIdTest, ParseWrongLength) {
    auto id_res = ChunkId::Parse("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85"); // 63 chars
    ASSERT_FALSE(id_res.IsOk());
    EXPECT_EQ(id_res.Error(), ChunkId::ParseError::WrongLength);
}

TEST(ChunkIdTest, ParseNotLowercase) {
    std::string hex = "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    auto id_res = ChunkId::Parse(hex);
    ASSERT_FALSE(id_res.IsOk());
    EXPECT_EQ(id_res.Error(), ChunkId::ParseError::NotLowercase);
}

TEST(ChunkIdTest, ParseInvalidChar) {
    std::string hex = "g3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    auto id_res = ChunkId::Parse(hex);
    ASSERT_FALSE(id_res.IsOk());
    EXPECT_EQ(id_res.Error(), ChunkId::ParseError::InvalidHexCharacter);
}

TEST(ChunkIdTest, HashFunctor) {
    std::string hex1 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    std::string hex2 = "0000000000000000000000000000000000000000000000000000000000000000";
    
    std::unordered_set<ChunkId, ChunkId::Hash> ids;
    ids.insert(ChunkId::Parse(hex1).Value());
    ids.insert(ChunkId::Parse(hex2).Value());
    EXPECT_EQ(ids.size(), 2);
}
