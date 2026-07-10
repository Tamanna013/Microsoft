#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "nimbus/common/config_schema.h"
#include "nimbus/common/result.h"

namespace nimbus::common {

enum class ConfigErrorCode {
  FileNotFound,
  MalformedJson,
  SchemaValidationFailed,
  ClusterInvariantEnvOverrideAttempted,
  KeyNotFound,
  TypeMismatch
};

struct ConfigError {
  ConfigErrorCode code;
  std::string message;
  std::vector<std::string> validation_errors;
};

class Config {
 public:
  static Result<Config, ConfigError> LoadFromFile(const std::filesystem::path& path);

  [[nodiscard]] Result<void, ConfigError> Validate(const ConfigSchema& schema) const;

  [[nodiscard]] Result<std::string, ConfigError> GetString(ConfigSection section,
                                                           const std::string& path) const;
  [[nodiscard]] std::string GetString(ConfigSection section, const std::string& path,
                                      const std::string& default_value) const;

  [[nodiscard]] Result<int64_t, ConfigError> GetInt64(ConfigSection section,
                                                      const std::string& path) const;
  [[nodiscard]] int64_t GetInt64(ConfigSection section, const std::string& path,
                                 int64_t default_value) const;

  [[nodiscard]] Result<double, ConfigError> GetDouble(ConfigSection section,
                                                      const std::string& path) const;
  [[nodiscard]] double GetDouble(ConfigSection section, const std::string& path,
                                 double default_value) const;

  [[nodiscard]] Result<bool, ConfigError> GetBool(ConfigSection section,
                                                  const std::string& path) const;
  [[nodiscard]] bool GetBool(ConfigSection section, const std::string& path,
                             bool default_value) const;

 private:
  Config() = default;
  nlohmann::json data_;

  [[nodiscard]] std::optional<std::string> GetEnvOverride(ConfigSection section,
                                                          const std::string& path) const;
};

}  // namespace nimbus::common
