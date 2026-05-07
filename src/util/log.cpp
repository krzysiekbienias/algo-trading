#include "util/log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <mutex>
#include <string>

namespace at::log {

namespace {

spdlog::level::level_enum parse_level(std::string_view s, spdlog::level::level_enum fallback) {
    if (s == "trace") return spdlog::level::trace;
    if (s == "debug") return spdlog::level::debug;
    if (s == "info") return spdlog::level::info;
    if (s == "warn" || s == "warning") return spdlog::level::warn;
    if (s == "error") return spdlog::level::err;
    if (s == "critical" || s == "crit") return spdlog::level::critical;
    if (s == "off") return spdlog::level::off;
    return fallback;
}

}  // namespace

void init(std::string_view default_level) {
    static std::once_flag flag;
    std::call_once(flag, [&] {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>("at", sink);

        // [HH:MM:SS.mmm] [level] [thread] message
        logger->set_pattern("[%H:%M:%S.%e] [%^%l%$] [t:%t] %v");

        spdlog::set_default_logger(logger);

        const char* env = std::getenv("AT_LOG_LEVEL");
        const auto fallback = parse_level(default_level, spdlog::level::info);
        spdlog::set_level(env ? parse_level(env, fallback) : fallback);
    });
}

}  // namespace at::log
