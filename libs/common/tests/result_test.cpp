#include <gtest/gtest.h>
#include "nimbus/common/result.h"
#include <string>

using namespace nimbus::common;

TEST(ResultTest, OkValue) {
    auto res = Result<int, std::string>::Ok(42);
    EXPECT_TRUE(res.IsOk());
    EXPECT_EQ(res.Value(), 42);
    EXPECT_THROW(res.Error(), std::logic_error);
}

TEST(ResultTest, ErrValue) {
    auto res = Result<int, std::string>::Err("failed");
    EXPECT_FALSE(res.IsOk());
    EXPECT_EQ(res.Error(), "failed");
    EXPECT_THROW(res.Value(), std::logic_error);
}
