#pragma once
#include <chrono>

namespace nimbus::common {

/**
 * @class Lease
 * @brief A uniform time-bounded expiry abstraction.
 * 
 * Thread-safety: Fully thread-safe. Instances are structurally immutable after creation.
 */
class Lease {
 public:
  static Lease IssueNow(std::chrono::system_clock::duration ttl,
                        std::chrono::system_clock::time_point now);

  [[nodiscard]] bool IsExpired(std::chrono::system_clock::time_point now,
                               std::chrono::system_clock::duration clock_skew_tolerance) const;

  [[nodiscard]] std::chrono::system_clock::time_point IssuedAt() const;
  [[nodiscard]] std::chrono::system_clock::time_point ExpiresAt() const;

 private:
  Lease(std::chrono::system_clock::time_point issued_at,
        std::chrono::system_clock::duration ttl);
  
  std::chrono::system_clock::time_point issued_at_;
  std::chrono::system_clock::duration ttl_;
};

}  // namespace nimbus::common
