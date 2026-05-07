#pragma once

#include <spdlog/spdlog.h>

#include <string_view>

namespace at::log {

// Initialize the global spdlog default logger with a sensible console sink
// and pattern. Idempotent: calling more than once is a no-op.
// Honors the AT_LOG_LEVEL env var (trace|debug|info|warn|error|critical|off).
void init(std::string_view default_level = "info");

}  // namespace at::log
