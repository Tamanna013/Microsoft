#include "nimbus/common/path_validator.h"

namespace nimbus::common {

PathValidator::PathValidator(PathValidatorConfig config)
    : config_(std::move(config)),
      compiled_allowed_pattern_("^(" + config_.allowed_character_pattern + ")+$") {}

Result<std::monostate, PathValidationError> PathValidator::Validate(std::string_view path) const {
  if (path.empty() || path[0] != '/') {
    return Result<std::monostate, PathValidationError>::Err(PathValidationError::NotAbsolute);
  }

  if (path.size() > config_.max_path_length) {
    return Result<std::monostate, PathValidationError>::Err(PathValidationError::PathTooLong);
  }

  for (char c : path) {
    if (c == '\0') {
      return Result<std::monostate, PathValidationError>::Err(PathValidationError::ContainsNullByte);
    }
  }

  // Handle root path specifically (FR: Vacuously true, 0 components)
  if (path == "/") {
    return Result<std::monostate, PathValidationError>::Ok({});
  }

  // Parse components
  size_t start = 1;
  while (start < path.size()) {
    size_t end = path.find('/', start);
    std::string_view component =
        path.substr(start, (end == std::string_view::npos) ? std::string_view::npos : end - start);

    if (component.empty()) {
      return Result<std::monostate, PathValidationError>::Err(PathValidationError::EmptyComponent);
    }

    if (component.size() > config_.max_component_length) {
      return Result<std::monostate, PathValidationError>::Err(PathValidationError::ComponentTooLong);
    }

    if (component == "..") {
      return Result<std::monostate, PathValidationError>::Err(
          PathValidationError::ContainsParentTraversal);
    }

    std::string comp_str(component);
    if (!std::regex_match(comp_str, compiled_allowed_pattern_)) {
      return Result<std::monostate, PathValidationError>::Err(PathValidationError::InvalidCharacter);
    }

    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return Result<std::monostate, PathValidationError>::Ok({});
}

std::string PathValidator::NormalizeForDisplay(std::string_view path) const {
  if (path.empty()) return "";
  std::string result;
  result.reserve(path.size());
  result.push_back(path[0]);
  for (size_t i = 1; i < path.size(); ++i) {
    if (path[i] == '/' && result.back() == '/') continue;
    result.push_back(path[i]);
  }
  return result;
}

}  // namespace nimbus::common
