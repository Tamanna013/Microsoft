#pragma once

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

namespace nimbus::common {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

struct LogField {
  std::string key;
  std::string value;
};

struct LoggerConfig {
  std::string component_name;
  std::filesystem::path log_directory;
  LogLevel level = LogLevel::Info;
  size_t rotation_max_bytes = 100 * 1024 * 1024;
  size_t rotation_max_files = 5;
};

class Logger {
 public:
  explicit Logger(LoggerConfig config);
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) noexcept;
  Logger& operator=(Logger&&) noexcept;

  void Trace(std::string_view message, std::initializer_list<LogField> fields = {});
  void Debug(std::string_view message, std::initializer_list<LogField> fields = {});
  void Info(std::string_view message, std::initializer_list<LogField> fields = {});
  void Warn(std::string_view message, std::initializer_list<LogField> fields = {});
  void Error(std::string_view message, std::initializer_list<LogField> fields = {});
  void Critical(std::string_view message, std::initializer_list<LogField> fields = {});

  void Audit(std::string_view actor, std::string_view action, std::string_view resource,
             std::string_view result, std::initializer_list<LogField> extra_fields = {});

  class ScopedCorrelationId {
   public:
    ~ScopedCorrelationId();
    ScopedCorrelationId(const ScopedCorrelationId&) = delete;
    ScopedCorrelationId& operator=(const ScopedCorrelationId&) = delete;

   private:
    friend class Logger;
    explicit ScopedCorrelationId(std::string previous_value);
    std::string previous_value_;
  };

  [[nodiscard]] ScopedCorrelationId WithCorrelationId(std::string correlation_id);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace nimbus::common
