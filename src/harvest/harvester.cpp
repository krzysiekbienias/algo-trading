#include "harvest/harvester.hpp"

#include <Contract.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ibkr/contract.hpp"
#include "ibkr/historical.hpp"
#include "ibkr/session.hpp"
#include "market/types.hpp"
#include "storage/data_lake_paths.hpp"
#include "storage/parquet_writer.hpp"
#include "util/time.hpp"

namespace at::harvest {

namespace {

using at::market::Bar;

std::chrono::milliseconds barPeriod(const std::string& bar_size) {
    if (bar_size == "1 min") {
        return std::chrono::minutes(1);
    }
    if (bar_size == "5 mins") {
        return std::chrono::minutes(5);
    }
    if (bar_size == "15 mins") {
        return std::chrono::minutes(15);
    }
    if (bar_size == "30 mins") {
        return std::chrono::minutes(30);
    }
    if (bar_size == "1 hour") {
        return std::chrono::hours(1);
    }
    if (bar_size == "1 day") {
        return std::chrono::hours(24);
    }
    throw std::invalid_argument("unsupported bar_size_setting: " + bar_size);
}

void monthYearFromTimestamp(at::time::Timestamp tp, int& year, int& month) {
    const auto ms = at::time::toEpochMs(tp);
    const std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm t{};
    if (gmtime_r(&sec, &t) == nullptr) {
        throw std::runtime_error("monthYearFromTimestamp: gmtime_r failed");
    }
    year = t.tm_year + 1900;
    month = t.tm_mon + 1;
}

void appendBarsByMonth(const std::filesystem::path& root, std::string_view symbol,
                       const std::vector<Bar>& bars) {
    if (bars.empty()) {
        return;
    }

    std::map<std::pair<int, int>, std::vector<Bar>> by_month;
    for (const auto& bar : bars) {
        int year = 0;
        int month = 0;
        monthYearFromTimestamp(bar.timestamp, year, month);
        by_month[{year, month}].push_back(bar);
    }

    for (auto& [ym, month_bars] : by_month) {
        const auto path =
            at::storage::pathFor(root, symbol, ym.first, ym.second);
        at::storage::ParquetWriter writer(path);
        writer.append(month_bars);
        spdlog::info("harvest: appended {} bars -> {}", month_bars.size(), path.string());
    }
}

std::optional<at::time::Timestamp> oldestTimestampInLake(const std::filesystem::path& root,
                                                         std::string_view symbol) {
    const auto dir = at::storage::symbolDir(root, symbol);
    if (!std::filesystem::exists(dir)) {
        return std::nullopt;
    }

    std::optional<at::time::Timestamp> oldest;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".parquet") {
            continue;
        }
        at::storage::ParquetWriter reader(entry.path());
        const auto bars = reader.readAll();
        if (bars.empty()) {
            continue;
        }
        const auto candidate = bars.front().timestamp;
        if (!oldest || candidate < *oldest) {
            oldest = candidate;
        }
    }
    return oldest;
}

at::time::Timestamp resolveInitialEnd(const std::filesystem::path& root,
                                      std::string_view symbol,
                                      const std::chrono::milliseconds& period) {
    if (const auto oldest = oldestTimestampInLake(root, symbol)) {
        return *oldest - period;
    }
    return at::time::now();
}

std::vector<Bar> requestChunkWithRetry(at::ibkr::Session& session, const Contract& contract,
                                       at::ibkr::HistoricalRequest req,
                                       const HarvesterConfig& config) {
    for (int attempt = 0; attempt <= config.max_pacing_retries; ++attempt) {
        try {
            return session.reqHistoricalBars(contract, req);
        } catch (const at::ibkr::PacingViolationError& e) {
            if (attempt >= config.max_pacing_retries) {
                throw;
            }
            spdlog::warn("harvest: pacing 162 (attempt {}/{}), sleeping {}s then retry",
                         attempt + 1, config.max_pacing_retries,
                         config.pacing_retry_sleep.count() / 1000);
            std::this_thread::sleep_for(config.pacing_retry_sleep);
        }
    }
    throw std::runtime_error("requestChunkWithRetry: unreachable");
}

}  // namespace

Contract makeContract(const HarvesterConfig& config) {
    return at::ibkr::makeStockSmart(config.symbol);
}

int run(at::ibkr::Session& session, const HarvesterConfig& config) {
    if (config.symbol.empty()) {
        throw std::invalid_argument("HarvesterConfig.symbol is required");
    }

    const auto period = barPeriod(config.bar_size_setting);
    at::storage::ensureSymbolDir(config.data_lake_root, config.symbol);

    auto end = resolveInitialEnd(config.data_lake_root, config.symbol, period);
    const auto target = config.history_start;

    if (end <= target) {
        spdlog::info("harvest: {} already covers history_start {}; nothing to do",
                     config.symbol, at::time::formatIso8601(target));
        return 0;
    }

    const Contract contract = makeContract(config);
    int total_bars = 0;
    int chunk_index = 0;

    spdlog::info("harvest: {} bar='{}' from {} backward to {}", config.symbol,
                 config.bar_size_setting, at::time::formatIso8601(end),
                 at::time::formatIso8601(target));

    while (end > target) {
        at::ibkr::HistoricalRequest req{
            .end_date_time    = at::time::formatTwsHistoricalEndUtc(end),
            .duration_str     = config.chunk_duration,
            .bar_size_setting = config.bar_size_setting,
            .what_to_show     = config.what_to_show,
            .use_rth          = config.use_rth,
            .format_date      = 1,
            .timeout          = std::chrono::seconds(90),
        };

        spdlog::info("harvest: chunk {} end={}", chunk_index, req.end_date_time);
        std::vector<Bar> bars =
            requestChunkWithRetry(session, contract, req, config);

        if (bars.empty()) {
            spdlog::info("harvest: empty chunk — stopping");
            break;
        }

        std::sort(bars.begin(), bars.end(),
                  [](const Bar& a, const Bar& b) {
                      return at::time::toEpochMs(a.timestamp) <
                             at::time::toEpochMs(b.timestamp);
                  });

        appendBarsByMonth(config.data_lake_root, config.symbol, bars);
        total_bars += static_cast<int>(bars.size());

        const auto oldest = bars.front().timestamp;
        spdlog::info("harvest: chunk {} oldest={}", chunk_index,
                     at::time::formatIso8601(oldest));

        if (oldest <= target) {
            spdlog::info("harvest: reached history_start");
            break;
        }

        end = oldest - period;
        ++chunk_index;

        spdlog::debug("harvest: sleeping {}ms before next request",
                      config.inter_request_sleep.count());
        std::this_thread::sleep_for(config.inter_request_sleep);
    }

    spdlog::info("harvest: done {} — {} bars appended in {} chunks", config.symbol,
                 total_bars, chunk_index + 1);
    return total_bars;
}

}  // namespace at::harvest
