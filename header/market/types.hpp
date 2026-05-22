#pragma once

#include "util/time.hpp"

namespace at::market {

// One OHLCV bar in real price units (open/high/low/close as doubles).
//
// Time policy: timestamp is UTC milliseconds since Unix epoch, stored in the
// canonical at::time::Timestamp type. Matches Apache Parquet timestamp[ms, UTC].
struct Bar {
    at::time::Timestamp timestamp{};
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

}  // namespace at::market
