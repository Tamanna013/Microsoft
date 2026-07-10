#include "nimbus/common/config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
extern char** environ;
#endif

namespace nimbus::common {

static bool HasClusterInvariantEnvOverrides() {
#ifdef _WIN32
  LPCH env_strings = GetEnvironmentStrings();
  if (env_strings == nullptr) return false;
  for (LPTSTR p = (LPTSTR)env_strings; *p; p += lstrlen(p) + 1) {
    std::string env_var(p);
    if (env_var.find("NIMBUS_CLUSTER_") == 0) {
      FreeEnvironmentStrings(env_strings);
      return true;
    }
  }
  FreeEnvironmentStrings(env_strings);
#else
  for (char** env = environ; *env != nullptr; ++env) {
    std::string env_var(*env);
    if (env_var.find("NIMBUS_CLUSTER_") == 0) {
      return true;
    }
  }
#endif
  return false;
}

static std::string ToUpperAndUnderscore(const std::string& input) {
  std::string result = input;
  std::replace(result.begin(), result.end(), '.', '_');
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return result;
}

Result<Config, ConfigError> Config::LoadFromFile(const std::filesystem::path& path) {
  if (HasClusterInvariantEnvOverrides()) {
    return Result<Config, ConfigError>::Err(ConfigError{
        ConfigErrorCode::ClusterInvariantEnvOverrideAttempted,
        "Attempted to override a cluster invariant configuration via environment variables",
        {}});
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<Config, ConfigError>::Err(
        ConfigError{ConfigErrorCode::FileNotFound, "Configuration file not found: " + path.string(), {}});
  }

  Config config;
  try {
    config.data_ = nlohmann::json::parse(file);
  } catch (const nlohmann::json::parse_error& e) {
    return Result<Config, ConfigError>::Err(
        ConfigError{ConfigErrorCode::MalformedJson, "Malformed JSON: " + std::string(e.what()), {}});
  }

  return Result<Config, ConfigError>::Ok(std::move(config));
}

Result<void, ConfigError> Config::Validate(const ConfigSchema& schema) const {
  std::vector<std::string> validation_errors;

  auto validate_section = [&](const std::map<std::string, ConfigFieldSpec>& specs,
                              ConfigSection section, const std::string& section_name) {
    for (const auto& [key, spec] : specs) {
      // For NodeLocal, check env first.
      bool has_env = false;
      if (section == ConfigSection::NodeLocal) {
         if (GetEnvOverride(section, key).has_value()) {
             has_env = true;
         }
      }

      if (!has_env) {
        if (!data_.contains(section_name) || !data_[section_name].contains(key)) {
          if (spec.required) {
            validation_errors.push_back("Missing required key: " + section_name + "." + key);
          }
          continue;
        }

        const auto& jval = data_[section_name][key];
        switch (spec.type) {
          case ConfigFieldType::String:
            if (!jval.is_string())
              validation_errors.push_back("Type mismatch for " + section_name + "." + key +
                                          ": expected string");
            break;
          case ConfigFieldType::Int64:
            if (!jval.is_number_integer())
              validation_errors.push_back("Type mismatch for " + section_name + "." + key +
                                          ": expected int64");
            break;
          case ConfigFieldType::Double:
            if (!jval.is_number_float() && !jval.is_number_integer())
              validation_errors.push_back("Type mismatch for " + section_name + "." + key +
                                          ": expected double");
            break;
          case ConfigFieldType::Bool:
            if (!jval.is_boolean())
              validation_errors.push_back("Type mismatch for " + section_name + "." + key +
                                          ": expected boolean");
            break;
        }
      }
    }
  };

  validate_section(schema.GetClusterInvariantSpecs(), ConfigSection::ClusterInvariant,
                   "cluster_invariant");
  validate_section(schema.GetNodeLocalSpecs(), ConfigSection::NodeLocal, "node_local");

  if (!validation_errors.empty()) {
    return Result<void, ConfigError>::Err(ConfigError{ConfigErrorCode::SchemaValidationFailed,
                                                      "Schema validation failed",
                                                      validation_errors});
  }

  return Result<void, ConfigError>::Ok({});
}

std::optional<std::string> Config::GetEnvOverride(ConfigSection section,
                                                  const std::string& path) const {
  if (section == ConfigSection::ClusterInvariant) {
    return std::nullopt;
  }
  std::string env_var_name = "NIMBUS_NODE_" + ToUpperAndUnderscore(path);
  const char* val = std::getenv(env_var_name.c_str());
  if (val) {
    return std::string(val);
  }
  return std::nullopt;
}

Result<std::string, ConfigError> Config::GetString(ConfigSection section,
                                                   const std::string& path) const {
  if (auto env_val = GetEnvOverride(section, path)) {
    return Result<std::string, ConfigError>::Ok(*env_val);
  }

  std::string section_name = section == ConfigSection::ClusterInvariant ? "cluster_invariant" : "node_local";
  if (!data_.contains(section_name) || !data_[section_name].contains(path)) {
    return Result<std::string, ConfigError>::Err(
        ConfigError{ConfigErrorCode::KeyNotFound, "Key not found: " + path, {}});
  }
  if (!data_[section_name][path].is_string()) {
    return Result<std::string, ConfigError>::Err(
        ConfigError{ConfigErrorCode::TypeMismatch, "Type mismatch, expected string", {}});
  }
  return Result<std::string, ConfigError>::Ok(data_[section_name][path].get<std::string>());
}

std::string Config::GetString(ConfigSection section, const std::string& path,
                              const std::string& default_value) const {
  auto res = GetString(section, path);
  if (res.IsOk()) return res.Value();
  return default_value;
}

Result<int64_t, ConfigError> Config::GetInt64(ConfigSection section, const std::string& path) const {
  if (auto env_val = GetEnvOverride(section, path)) {
    try {
      return Result<int64_t, ConfigError>::Ok(std::stoll(*env_val));
    } catch (...) {
      return Result<int64_t, ConfigError>::Err(
          ConfigError{ConfigErrorCode::TypeMismatch, "Env var could not be parsed as int64", {}});
    }
  }

  std::string section_name = section == ConfigSection::ClusterInvariant ? "cluster_invariant" : "node_local";
  if (!data_.contains(section_name) || !data_[section_name].contains(path)) {
    return Result<int64_t, ConfigError>::Err(
        ConfigError{ConfigErrorCode::KeyNotFound, "Key not found: " + path, {}});
  }
  if (!data_[section_name][path].is_number_integer()) {
    return Result<int64_t, ConfigError>::Err(
        ConfigError{ConfigErrorCode::TypeMismatch, "Type mismatch, expected int64", {}});
  }
  return Result<int64_t, ConfigError>::Ok(data_[section_name][path].get<int64_t>());
}

int64_t Config::GetInt64(ConfigSection section, const std::string& path,
                         int64_t default_value) const {
  auto res = GetInt64(section, path);
  if (res.IsOk()) return res.Value();
  return default_value;
}

Result<double, ConfigError> Config::GetDouble(ConfigSection section, const std::string& path) const {
  if (auto env_val = GetEnvOverride(section, path)) {
    try {
      return Result<double, ConfigError>::Ok(std::stod(*env_val));
    } catch (...) {
      return Result<double, ConfigError>::Err(
          ConfigError{ConfigErrorCode::TypeMismatch, "Env var could not be parsed as double", {}});
    }
  }

  std::string section_name = section == ConfigSection::ClusterInvariant ? "cluster_invariant" : "node_local";
  if (!data_.contains(section_name) || !data_[section_name].contains(path)) {
    return Result<double, ConfigError>::Err(
        ConfigError{ConfigErrorCode::KeyNotFound, "Key not found: " + path, {}});
  }
  if (!data_[section_name][path].is_number()) {
    return Result<double, ConfigError>::Err(
        ConfigError{ConfigErrorCode::TypeMismatch, "Type mismatch, expected double", {}});
  }
  return Result<double, ConfigError>::Ok(data_[section_name][path].get<double>());
}

double Config::GetDouble(ConfigSection section, const std::string& path,
                         double default_value) const {
  auto res = GetDouble(section, path);
  if (res.IsOk()) return res.Value();
  return default_value;
}

Result<bool, ConfigError> Config::GetBool(ConfigSection section, const std::string& path) const {
  if (auto env_val = GetEnvOverride(section, path)) {
    std::string lower = *env_val;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "true" || lower == "1") return Result<bool, ConfigError>::Ok(true);
    if (lower == "false" || lower == "0") return Result<bool, ConfigError>::Ok(false);
    return Result<bool, ConfigError>::Err(
        ConfigError{ConfigErrorCode::TypeMismatch, "Env var could not be parsed as bool", {}});
  }

  std::string section_name = section == ConfigSection::ClusterInvariant ? "cluster_invariant" : "node_local";
  if (!data_.contains(section_name) || !data_[section_name].contains(path)) {
    return Result<bool, ConfigError>::Err(
        ConfigError{ConfigErrorCode::KeyNotFound, "Key not found: " + path, {}});
  }
  if (!data_[section_name][path].is_boolean()) {
    return Result<bool, ConfigError>::Err(
        ConfigError{ConfigErrorCode::TypeMismatch, "Type mismatch, expected bool", {}});
  }
  return Result<bool, ConfigError>::Ok(data_[section_name][path].get<bool>());
}

bool Config::GetBool(ConfigSection section, const std::string& path, bool default_value) const {
  auto res = GetBool(section, path);
  if (res.IsOk()) return res.Value();
  return default_value;
}

}  // namespace nimbus::common
