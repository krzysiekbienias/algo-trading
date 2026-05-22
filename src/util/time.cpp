#include "util/time.hpp"

#include <fmt/chrono.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace at::time {

std::string formatIso8601(Timestamp tp) {
    // {:%FT%T} on sys_time<milliseconds> renders as "2026-05-06T17:42:13.123"
    // with sub-second precision baked in by the chrono formatter — no manual
    // ms-extraction needed. We append the explicit 'Z' UTC marker.
    return fmt::format("{:%FT%T}Z", tp);
}

std::optional<Timestamp> parseIso8601(std::string_view s) {
    if (s.empty()) return std::nullopt;

    std::string buf(s);

    // Strip optional trailing 'Z' UTC marker (RFC-3339).
    if (buf.back() == 'Z') buf.pop_back();
    if (buf.empty()) return std::nullopt;

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, sec = 0, ms = 0;

    if (buf.size() == 10 && buf[4] == '-' && buf[7] == '-') {
        // Date-only: "2026-05-06"
        if (std::sscanf(buf.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
            return std::nullopt;
    } else if (buf.size() >= 19) {
        // Full datetime with 'T' or space separator.
        char sep = buf[10];
        if (sep != 'T' && sep != ' ') return std::nullopt;

        // Replace separator with a space so sscanf can use a single format.
        buf[10] = ' ';

        int parsed = std::sscanf(buf.c_str(), "%d-%d-%d %d:%d:%d",
                                 &year, &month, &day, &hour, &minute, &sec);
        if (parsed != 6) return std::nullopt;

        // Optional sub-second part: ".123"
        if (buf.size() > 19 && buf[19] == '.') {
            std::string ms_str = buf.substr(20, 3);
            while (ms_str.size() < 3) ms_str += '0';
            ms = std::stoi(ms_str);
        }
    } else {
        return std::nullopt;
    }

    // Basic range validation before feeding to timegm.
    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        sec < 0 || sec > 60 || ms < 0 || ms > 999) {
        return std::nullopt;
    }

    struct tm t{};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = sec;
    t.tm_isdst = 0;

    // timegm interprets tm as UTC (unlike mktime which uses local zone).
    const time_t epoch_sec = timegm(&t);
    if (epoch_sec == static_cast<time_t>(-1)) return std::nullopt;

    return Timestamp{
        std::chrono::milliseconds{static_cast<std::int64_t>(epoch_sec) * 1000 + ms}};
}

std::string formatTwsHistoricalEndUtc(Timestamp tp) {
    const auto ms = toEpochMs(tp);
    const std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm t{};
    if (gmtime_r(&sec, &t) == nullptr) {
        throw std::runtime_error("formatTwsHistoricalEndUtc: gmtime_r failed");
    }
    char buf[32];
    if (std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d:%02d:%02d", t.tm_year + 1900,
                      t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec) <= 0) {
        throw std::runtime_error("formatTwsHistoricalEndUtc: snprintf failed");
    }
    return buf;
}

}  // namespace at::time
