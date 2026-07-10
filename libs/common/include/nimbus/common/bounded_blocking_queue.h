#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include "nimbus/common/result.h"

namespace nimbus::common {

/**
 * @class BoundedBlockingQueue
 * @brief Fixed-capacity thread-safe producer-consumer queue.
 * 
 * Demonstrates hand-built synchronization using std::mutex and std::condition_variable.
 */
template <typename T>
class BoundedBlockingQueue {
 public:
  enum class QueueError { ShutDown, Timeout };

  explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
      throw std::logic_error("BoundedBlockingQueue capacity cannot be zero");
    }
  }

  [[nodiscard]] Result<std::monostate, QueueError> Push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    not_full_.wait(lock, [this]() { return shutdown_ || items_.size() < capacity_; });
    if (shutdown_) return Result<std::monostate, QueueError>::Err(QueueError::ShutDown);
    items_.push(std::move(item));
    not_empty_.notify_one();
    return Result<std::monostate, QueueError>::Ok({});
  }

  [[nodiscard]] std::optional<T> Pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this]() { return shutdown_ || !items_.empty(); });
    if (shutdown_ && items_.empty()) return std::nullopt;
    
    T item = std::move(items_.front());
    items_.pop();
    not_full_.notify_one();
    return item;
  }

  [[nodiscard]] Result<std::monostate, QueueError> TryPush(T item, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!not_full_.wait_for(lock, timeout, [this]() { return shutdown_ || items_.size() < capacity_; })) {
      return Result<std::monostate, QueueError>::Err(QueueError::Timeout);
    }
    if (shutdown_) return Result<std::monostate, QueueError>::Err(QueueError::ShutDown);
    items_.push(std::move(item));
    not_empty_.notify_one();
    return Result<std::monostate, QueueError>::Ok({});
  }

  [[nodiscard]] Result<std::optional<T>, QueueError> TryPop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!not_empty_.wait_for(lock, timeout, [this]() { return shutdown_ || !items_.empty(); })) {
      return Result<std::optional<T>, QueueError>::Err(QueueError::Timeout);
    }
    if (shutdown_ && items_.empty()) return Result<std::optional<T>, QueueError>::Ok(std::nullopt);
    
    T item = std::move(items_.front());
    items_.pop();
    not_full_.notify_one();
    return Result<std::optional<T>, QueueError>::Ok(std::move(item));
  }

  void Shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    shutdown_ = true;
    not_full_.notify_all();
    not_empty_.notify_all();
  }

  /**
   * @brief Approximate size for diagnostics only.
   * Never use this value for control-flow decisions.
   */
  [[nodiscard]] size_t SizeApprox() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return items_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable not_full_;
  std::condition_variable not_empty_;
  std::queue<T> items_;
  size_t capacity_;
  bool shutdown_ = false;
};

}  // namespace nimbus::common
