#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

#include "util/time.hpp"

struct Contract;

namespace at::ibkr {
class Session;
}

namespace at::harvest {

struct HarvesterConfig {
    std::filesystem::path data_lake_root = "data_lake";
    std::string symbol;
    std::string bar_size_setting = "1 hour";
    at::time::Timestamp history_start{};

    // One reqHistoricalData window per iteration (backward from end).
    std::string chunk_duration = "2 W";

    std::chrono::milliseconds inter_request_sleep{std::chrono::seconds(12)};
    std::chrono::milliseconds pacing_retry_sleep{std::chrono::seconds(60)};
    int max_pacing_retries = 5;

    std::string what_to_show = "TRADES";
    int use_rth = 1;
};

// Backfill OHLCV from TWS into monthly Parquet shards under data_lake/.
// Session must already be connected. Returns total bars appended.
int run(at::ibkr::Session& session, const HarvesterConfig& config);

// Build STK/SMART contract for the configured symbol (MVP).
Contract makeContract(const HarvesterConfig& config);

}  // namespace at::harvest
