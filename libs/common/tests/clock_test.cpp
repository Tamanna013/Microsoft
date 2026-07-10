#include <gtest/gtest.h>
#include "nimbus/common/clock.h"

using namespace nimbus::common;

TEST(FakeClockTest, AdvanceAndSetTo) {
    auto start = std::chrono::system_clock::now();
    FakeClock clock(start);
    EXPECT_EQ(clock.Now(), start);

    clock.Advance(std::chrono::seconds(10));
    EXPECT_EQ(clock.Now(), start + std::chrono::seconds(10));

    auto target = start + std::chrono::seconds(100);
    clock.SetTo(target);
    EXPECT_EQ(clock.Now(), target);
}

TEST(RealClockTest, NowIsSane) {
    RealClock clock;
    auto clock_now = clock.Now();
    auto sys_now = std::chrono::system_clock::now();
    
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(sys_now - clock_now).count();
    EXPECT_LE(std::abs(diff), 1000);
}
