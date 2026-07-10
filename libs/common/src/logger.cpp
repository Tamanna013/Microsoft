#include "nimbus/common/logger.h"

#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

namespace nimbus::common {

thread_local std::string t_correlation_id = "";

struct Logger::Impl {
  std::shared_ptr<spdlog::logger> op_logger;
  std::shared_ptr<spdlog::logger> audit_logger;
  std::string component_name;
};

static spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::Trace:
      return spdlog::level::trace;
    case LogLevel::Debug:
      return spdlog::level::debug;
    case LogLevel::Info:
      return spdlog::level::info;
    case LogLevel::Warn:
      return spdlog::level::warn;
    case LogLevel::Error:
      return spdlog::level::err;
    case LogLevel::Critical:
      return spdlog::level::critical;
  }
  return spdlog::level::info;
}

Logger::Logger(LoggerConfig config) : impl_(std::make_unique<Impl>()) {
  impl_->component_name = config.component_name;

  std::vector<spdlog::sink_ptr> op_sinks;
  op_sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  if (!config.log_directory.empty()) {
    std::filesystem::create_directories(config.log_directory);
    auto file_path = config.log_directory / (config.component_name + ".log");
    op_sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        file_path.string(), config.rotation_max_bytes, config.rotation_max_files));
  }

  impl_->op_logger = std::make_shared<spdlog::logger>(config.component_name, op_sinks.begin(),
                                                      op_sinks.end());

  // Pattern: [timestamp] [level] [component_name] message
  // We manually insert correlation_id and fields into message.
  impl_->op_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
  impl_->op_logger->set_level(ToSpdlogLevel(config.level));

  // Audit logger
  if (!config.log_directory.empty()) {
    auto audit_path = config.log_directory / (config.component_name + "_audit.log");
    auto audit_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        audit_path.string(), config.rotation_max_bytes, config.rotation_max_files);
    impl_->audit_logger =
        std::make_shared<spdlog::logger>(config.component_name + "_audit", audit_sink);
    // Audit pattern: [timestamp] message
    impl_->audit_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
    impl_->audit_logger->set_level(spdlog::level::info);
  }
}

Logger::~Logger() {
  if (impl_->op_logger) {
    impl_->op_logger->flush();
  }
  if (impl_->audit_logger) {
    impl_->audit_logger->flush();
  }
}

Logger::Logger(Logger&&) noexcept = default;
Logger& Logger::operator=(Logger&&) noexcept = default;

static std::string FormatFields(std::initializer_list<LogField> fields) {
  if (fields.size() == 0) {
    return "";
  }
  std::string result = " {";
  bool first = true;
  for (const auto& field : fields) {
    if (!first) {
      result += ", ";
    }
    result += fmt::format("{}={}", field.key, field.value);
    first = false;
  }
  result += "}";
  return result;
}

static std::string FormatOpMessage(std::string_view message,
                                   std::initializer_list<LogField> fields) {
  std::string corr_part = t_correlation_id.empty() ? "" : fmt::format("[{}] ", t_correlation_id);
  return fmt::format("{}{}{}", corr_part, message, FormatFields(fields));
}

void Logger::Trace(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->trace(FormatOpMessage(message, fields));
}
void Logger::Debug(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->debug(FormatOpMessage(message, fields));
}
void Logger::Info(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->info(FormatOpMessage(message, fields));
}
void Logger::Warn(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->warn(FormatOpMessage(message, fields));
}
void Logger::Error(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->error(FormatOpMessage(message, fields));
}
void Logger::Critical(std::string_view message, std::initializer_list<LogField> fields) {
  if (impl_->op_logger) impl_->op_logger->critical(FormatOpMessage(message, fields));
}

void Logger::Audit(std::string_view actor, std::string_view action, std::string_view resource,
                   std::string_view result, std::initializer_list<LogField> extra_fields) {
  if (impl_->audit_logger) {
    std::string extra = FormatFields(extra_fields);
    std::string msg = fmt::format("actor={} action={} resource={} result={}{}", actor, action,
                                  resource, result, extra);
    impl_->audit_logger->info(msg);
  }
}

Logger::ScopedCorrelationId::ScopedCorrelationId(std::string previous_value)
    : previous_value_(std::move(previous_value)) {}

Logger::ScopedCorrelationId::~ScopedCorrelationId() {
  t_correlation_id = std::move(previous_value_);
}

Logger::ScopedCorrelationId Logger::WithCorrelationId(std::string correlation_id) {
  ScopedCorrelationId scoped(t_correlation_id);
  t_correlation_id = std::move(correlation_id);
  return scoped;
}

}  // namespace nimbus::common
