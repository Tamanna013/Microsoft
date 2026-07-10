#pragma once
#include <chrono>

namespace nimbus::common {

/**
 * @class IClock
 * @brief Dependency-injectable clock interface for retrieving current time.
 */
class IClock {
 public:
  virtual ~IClock() = default;
  virtual std::chrono::system_clock::time_point Now() const = 0;
};

class RealClock final : public IClock {
 public:
  std::chrono::system_clock::time_point Now() const override;
};

/**
 * @class FakeClock
 * @brief Test-only fake clock.
 * 
 * Thread-safety: Explicitly NOT thread-safe. Intended for single-threaded test drivers.
 */
class FakeClock final : public IClock {
 public:
  explicit FakeClock(std::chrono::system_clock::time_point start);
  std::chrono::system_clock::time_point Now() const override;
  void Advance(std::chrono::system_clock::duration by);
  void SetTo(std::chrono::system_clock::time_point when);
 private:
  std::chrono::system_clock::time_point current_;
};

}  // namespace nimbus::common
