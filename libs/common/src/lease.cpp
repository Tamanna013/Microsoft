#include "nimbus/common/lease.h"

namespace nimbus::common {

Lease Lease::IssueNow(std::chrono::system_clock::duration ttl,
                      std::chrono::system_clock::time_point now) {
  return Lease(now, ttl);
}

Lease::Lease(std::chrono::system_clock::time_point issued_at,
             std::chrono::system_clock::duration ttl)
    : issued_at_(issued_at), ttl_(ttl) {}

bool Lease::IsExpired(std::chrono::system_clock::time_point now,
                      std::chrono::system_clock::duration clock_skew_tolerance) const {
  return now > (issued_at_ + ttl_ + clock_skew_tolerance);
}

std::chrono::system_clock::time_point Lease::IssuedAt() const {
  return issued_at_;
}

std::chrono::system_clock::time_point Lease::ExpiresAt() const {
  return issued_at_ + ttl_;
}

}  // namespace nimbus::common
