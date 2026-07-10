#include "nimbus/common/clock.h"

namespace nimbus::common {

std::chrono::system_clock::time_point RealClock::Now() const {
  return std::chrono::system_clock::now();
}

FakeClock::FakeClock(std::chrono::system_clock::time_point start) : current_(start) {}

std::chrono::system_clock::time_point FakeClock::Now() const {
  return current_;
}

void FakeClock::Advance(std::chrono::system_clock::duration by) {
  current_ += by;
}

void FakeClock::SetTo(std::chrono::system_clock::time_point when) {
  current_ = when;
}

}  // namespace nimbus::common
