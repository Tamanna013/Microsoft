#pragma once
#include <regex>
#include <string>
#include <string_view>
#include "nimbus/common/result.h"

namespace nimbus::common {

enum class PathValidationError {
  NotAbsolute,
  ContainsParentTraversal,
  ContainsNullByte,
  ComponentTooLong,
  PathTooLong,
  InvalidCharacter,
  EmptyComponent
};

struct PathValidatorConfig {
  size_t max_component_length = 255;
  size_t max_path_length = 4096;
  std::string allowed_character_pattern = R"([A-Za-z0-9._\- ])";
};

class PathValidator {
 public:
  explicit PathValidator(PathValidatorConfig config = {});

  [[nodiscard]] Result<std::monostate, PathValidationError> Validate(std::string_view path) const;

  /**
   * @brief Normalizes a path for display/logging only.
   * Do NOT use this for security-relevant comparisons or storage key derivation.
   */
  [[nodiscard]] std::string NormalizeForDisplay(std::string_view path) const;

 private:
  PathValidatorConfig config_;
  std::regex compiled_allowed_pattern_;
};

}  // namespace nimbus::common
