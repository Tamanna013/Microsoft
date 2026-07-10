#include <gtest/gtest.h>
#include "nimbus/common/protocol_version.h"

using namespace nimbus::common;

TEST(ProtocolVersionTest, ExactMatchAcceptance) {
    auto res = CheckProtocolVersion(1, 1, 1);
    EXPECT_TRUE(res.IsOk());
}

TEST(ProtocolVersionTest, WithinRangeAcceptance) {
    auto res = CheckProtocolVersion(2, 1, 3);
    EXPECT_TRUE(res.IsOk());
}

TEST(ProtocolVersionTest, BelowMinRejection) {
    auto res = CheckProtocolVersion(0, 1, 3);
    EXPECT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), ProtocolVersionError::Unsupported);
}

TEST(ProtocolVersionTest, AboveMaxRejection) {
    auto res = CheckProtocolVersion(4, 1, 3);
    EXPECT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), ProtocolVersionError::Unsupported);
}

TEST(ProtocolVersionTest, BoundaryExactMin) {
    auto res = CheckProtocolVersion(1, 1, 3);
    EXPECT_TRUE(res.IsOk());
}

TEST(ProtocolVersionTest, BoundaryExactMax) {
    auto res = CheckProtocolVersion(3, 1, 3);
    EXPECT_TRUE(res.IsOk());
}
