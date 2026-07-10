#include <gtest/gtest.h>
#include "nimbus/common/lease.h"

using namespace nimbus::common;

TEST(LeaseTest, ExpiryLogic) {
    auto start = std::chrono::system_clock::time_point{};
    auto ttl = std::chrono::seconds(10);
    auto lease = Lease::IssueNow(ttl, start);

    EXPECT_EQ(lease.IssuedAt(), start);
    EXPECT_EQ(lease.ExpiresAt(), start + ttl);

    auto tolerance = std::chrono::seconds(2);
    
    // Not yet expired
    EXPECT_FALSE(lease.IsExpired(start + std::chrono::seconds(5), tolerance));

    // Exactly at expiry boundary (not expired yet, only expired AFTER expires_at + tolerance)
    EXPECT_FALSE(lease.IsExpired(start + std::chrono::seconds(12), tolerance));

    // First instant after expires_at + tolerance IS expired
    EXPECT_TRUE(lease.IsExpired(start + std::chrono::seconds(12) + std::chrono::nanoseconds(1), tolerance));
    
    // Expired beyond tolerance
    EXPECT_TRUE(lease.IsExpired(start + std::chrono::seconds(15), tolerance));
}
