#pragma once

#include <stdexcept>
#include <utility>
#include <variant>

namespace nimbus::common {

template <typename T, typename E>
class Result {
 public:
  static Result Ok(T value) { return Result(std::in_place_index<0>, std::move(value)); }

  static Result Err(E error) { return Result(std::in_place_index<1>, std::move(error)); }

  [[nodiscard]] bool IsOk() const noexcept { return data_.index() == 0; }

  T& Value() {
    if (!IsOk()) {
      throw std::logic_error("Called Value() on an error Result");
    }
    return std::get<0>(data_);
  }

  const T& Value() const {
    if (!IsOk()) {
      throw std::logic_error("Called Value() on an error Result");
    }
    return std::get<0>(data_);
  }

  E& Error() {
    if (IsOk()) {
      throw std::logic_error("Called Error() on an ok Result");
    }
    return std::get<1>(data_);
  }

  const E& Error() const {
    if (IsOk()) {
      throw std::logic_error("Called Error() on an ok Result");
    }
    return std::get<1>(data_);
  }

 private:
  template <size_t I, typename... Args>
  explicit Result(std::in_place_index_t<I> idx, Args&&... args)
      : data_(idx, std::forward<Args>(args)...) {}

  std::variant<T, E> data_;
};

}  // namespace nimbus::common
