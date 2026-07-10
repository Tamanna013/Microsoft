#include <gtest/gtest.h>
#include "nimbus/common/sha256.h"

using namespace nimbus::common;

TEST(Sha256Test, KnownAnswerEmpty) {
    auto digest = Sha256::HashBytes(std::span<const std::byte>());
    std::string hex = Sha256::ToHexString(digest);
    EXPECT_EQ(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, KnownAnswerABC) {
    std::string_view abc = "abc";
    auto digest = Sha256::HashBytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(abc.data()), abc.size()));
    std::string hex = Sha256::ToHexString(digest);
    EXPECT_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, ChunkingIndependence) {
    std::string data = "this is a longer string that we will split into chunks for hashing";
    Sha256 hasher;
    hasher.Update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), 10));
    hasher.Update(std::span<const std::byte>()); // zero-length span
    hasher.Update(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data() + 10), data.size() - 10));
    auto chunked_digest = hasher.Finalize();

    auto single_digest = Sha256::HashBytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()));
    EXPECT_EQ(chunked_digest, single_digest);
}

TEST(Sha256Test, ToFromHexRoundTrip) {
    std::string_view abc = "abc";
    auto digest = Sha256::HashBytes(std::span<const std::byte>(reinterpret_cast<const std::byte*>(abc.data()), abc.size()));
    std::string hex = Sha256::ToHexString(digest);
    
    auto decoded_res = Sha256::FromHexString(hex);
    ASSERT_TRUE(decoded_res.IsOk());
    EXPECT_EQ(decoded_res.Value(), digest);
}

TEST(Sha256Test, FromHexErrors) {
    EXPECT_FALSE(Sha256::FromHexString("too_short").IsOk());
    EXPECT_FALSE(Sha256::FromHexString(std::string(64, 'z')).IsOk()); // invalid chars
}

TEST(Sha256Test, Preconditions) {
    Sha256 hasher;
    hasher.Finalize();
    EXPECT_THROW(hasher.Finalize(), std::logic_error); // double-finalize
    EXPECT_THROW(hasher.Update(std::span<const std::byte>()), std::logic_error); // update-after-finalize
}

TEST(Sha256Test, Benchmark) {
    // 100MB of data
    constexpr size_t size = 100 * 1024 * 1024;
    std::vector<std::byte> buf(size, std::byte{0x42});
    
    auto start = std::chrono::high_resolution_clock::now();
    Sha256::HashBytes(buf);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    double mb_per_sec = (100.0 * 1000.0) / static_cast<double>(diff);
    std::cout << "[ BENCHMARK ] Sha256 Throughput: " << mb_per_sec << " MB/s\n";
    // 500 MB/s floor
    EXPECT_GE(mb_per_sec, 500.0);
}
