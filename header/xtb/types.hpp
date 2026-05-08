#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "util/time.hpp"

namespace at::xtb {

// Credentials used in XTB's `login` command. Loaded from .env, never
// hard-coded.
struct Credentials {
    std::string user_id;
    std::string password;
    std::string app_name = "algo-trading-dev";
};

// Connection-level settings.
struct ClientConfig {
    std::string endpoint = "wss://ws.xapi.pro/demo";
    // Minimum delay between outgoing requests. XTB recommends >= 200 ms;
    // we default to 250 ms for safety margin.
    std::chrono::milliseconds throttle = std::chrono::milliseconds(250);
    // Hard timeout for a single request/response round-trip.
    std::chrono::milliseconds request_timeout = std::chrono::seconds(15);
};

// XTB chart period codes (minutes per bar). See `getChartRangeRequest` docs.
enum class Period : int {
    M1 = 1,
    M5 = 5,
    M15 = 15,
    M30 = 30,
    H1 = 60,
    H4 = 240,
    D1 = 1440,
    W1 = 10080,
    MN1 = 43200,
};

// One OHLCV bar after price normalization (open/high/low/close in real units,
// not XTB's "price * 10^digits" raw form).
//
// Time policy: we store the raw value XTB returns in `ctm` (milliseconds
// since Unix epoch, UTC) inside a strong chrono type. This keeps the wire
// representation lossless and makes every conversion ("seconds? ms? local
// time?") explicit at the call site. See header/util/time.hpp.
struct Bar {
    at::time::Timestamp timestamp{};
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

}  // namespace at::xtb
