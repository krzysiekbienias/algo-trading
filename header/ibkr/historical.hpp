#pragma once

#include <chrono>
#include <stdexcept>
#include <string>

namespace at::ibkr {

// Parameters for one reqHistoricalData call (single chunk).
struct HistoricalRequest {
    // Empty = current time. Otherwise "YYYYMMDD HH:mm:ss" UTC (TWS format).
    std::string end_date_time;
    // e.g. "1 D", "2 W", "1 M"
    std::string duration_str = "1 D";
    // e.g. "1 hour", "1 day", "1 min"
    std::string bar_size_setting = "1 hour";
    std::string what_to_show = "TRADES";
    int use_rth = 1;
    // 1 = string dates in callbacks, 2 = epoch seconds in bar.time
    int format_date = 1;
    std::chrono::milliseconds timeout = std::chrono::seconds(60);
};

// Thrown when TWS returns error 162 (pacing / rate limit). Caller may retry.
class PacingViolationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace at::ibkr
