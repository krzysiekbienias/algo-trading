// Scratchpad executable for ad-hoc experiments. Built only when
// ENABLE_DEV_MAIN=ON. Promote tested ideas into app/main.cpp when stable.

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "util/env.hpp"
#include "util/log.hpp"
#include "util/time.hpp"

#if defined(ENABLE_IBKR)
#include "harvest/harvester.hpp"
#include "ibkr/contract.hpp"
#include "ibkr/historical.hpp"
#include "ibkr/session.hpp"
#endif

namespace {

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " --ibkr-check\n"
              << "       " << argv0 << " --harvest-test [SYMBOL]\n"
              << "       " << argv0 << " --harvest --symbol SYMBOL --from DATE [options]\n"
              << "       " << argv0 << " --help\n"
              << "\n"
              << "IBKR TWS (requires TWS / IB Gateway with API enabled):\n"
              << "  --ibkr-check       Connect and call reqCurrentTime.\n"
              << "  --harvest-test     Fetch 1 day of 1-hour bars (default symbol: AAPL).\n"
              << "  --harvest          Backfill into data_lake/ (monthly Parquet shards).\n"
              << "\n"
              << "Harvest options:\n"
              << "  --symbol SYMBOL    Required, e.g. AAPL, NVDA.\n"
              << "  --from DATE        Required ISO date, e.g. 2024-01-01.\n"
              << "  --bar-size SIZE    Default: \"1 hour\".\n"
              << "  --data-lake PATH   Default: data_lake.\n"
              << "  --chunk DURATION   IBKR duration per request, default: 2 W.\n"
              << "\n"
              << "Env: IBKR_HOST, IBKR_PORT, IBKR_CLIENT_ID\n"
              << "      HARVEST_SLEEP_MS (default 12000), HARVEST_PACING_SLEEP_MS (60000)\n"
              << "\n"
              << "Paper trading default port is 7497; live is 7496.\n";
}

std::filesystem::path find_env_file() {
    namespace fs = std::filesystem;
    fs::path here = fs::current_path();
    fs::path fallback;
    for (int depth = 0; depth < 6; ++depth) {
        if (fs::exists(here / ".env")) {
            return here / ".env";
        }
        if (fs::exists(here / ".env.example") && fallback.empty()) {
            fallback = here / ".env.example";
        }
        if (here.has_parent_path() && here != here.parent_path()) {
            here = here.parent_path();
        } else {
            break;
        }
    }
    if (!fallback.empty()) {
        return fallback;
    }
    throw std::runtime_error("No .env or .env.example found in cwd or parents");
}

#if defined(ENABLE_IBKR)
at::ibkr::SessionConfig load_ibkr_config() {
    return at::ibkr::SessionConfig{
        .host      = at::env::getOr("IBKR_HOST", "127.0.0.1"),
        .port      = std::stoi(at::env::getOr("IBKR_PORT", "7497")),
        .client_id = std::stoi(at::env::getOr("IBKR_CLIENT_ID", "1")),
    };
}

int run_ibkr_check() {
    const auto env_path = find_env_file();
    spdlog::info("Loading env from: {}", env_path.string());
    at::env::loadIntoProcess(env_path);

    at::ibkr::Session session(load_ibkr_config());
    session.connect();
    const auto server_time = session.reqCurrentTime();
    session.disconnect();

    spdlog::info("IBKR TWS server time (epoch sec): {}", server_time);
    return 0;
}

int run_harvest_test(std::string_view symbol) {
    const auto env_path = find_env_file();
    spdlog::info("Loading env from: {}", env_path.string());
    at::env::loadIntoProcess(env_path);

    at::ibkr::Session session(load_ibkr_config());
    session.connect();

    const Contract contract = at::ibkr::makeStockSmart(std::string(symbol));
    at::ibkr::HistoricalRequest req{
        .end_date_time    = "",
        .duration_str     = "1 D",
        .bar_size_setting = "1 hour",
        .what_to_show     = "TRADES",
        .use_rth          = 1,
        .format_date      = 1,
        .timeout          = std::chrono::seconds(90),
    };

    const auto bars = session.reqHistoricalBars(contract, req);
    session.disconnect();

    spdlog::info("harvest-test: {} bars for {}", bars.size(), symbol);
    if (!bars.empty()) {
        spdlog::info("harvest-test: first ts epoch_ms={}",
                     at::time::toEpochMs(bars.front().timestamp));
        spdlog::info("harvest-test: last ts epoch_ms={}",
                     at::time::toEpochMs(bars.back().timestamp));
    }
    return bars.empty() ? 1 : 0;
}

struct HarvestCliArgs {
    std::string symbol;
    std::string from_date;
    std::string bar_size = "1 hour";
    std::filesystem::path data_lake = "data_lake";
    std::string chunk_duration = "2 W";
};

HarvestCliArgs parse_harvest_args(int argc, char** argv) {
    HarvestCliArgs args;
    bool seen_harvest = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--harvest") {
            seen_harvest = true;
            continue;
        }
        if (flag == "--symbol") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--symbol requires a value");
            }
            args.symbol = argv[++i];
            continue;
        }
        if (flag == "--from") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--from requires a value");
            }
            args.from_date = argv[++i];
            continue;
        }
        if (flag == "--bar-size") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--bar-size requires a value");
            }
            args.bar_size = argv[++i];
            continue;
        }
        if (flag == "--data-lake") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--data-lake requires a value");
            }
            args.data_lake = argv[++i];
            continue;
        }
        if (flag == "--chunk") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--chunk requires a value");
            }
            args.chunk_duration = argv[++i];
            continue;
        }
        throw std::runtime_error("Unknown harvest argument: " + std::string(flag));
    }

    if (!seen_harvest) {
        throw std::runtime_error("parse_harvest_args called without --harvest");
    }
    if (args.symbol.empty() || args.from_date.empty()) {
        throw std::runtime_error("--harvest requires --symbol and --from");
    }
    return args;
}

int run_harvest(int argc, char** argv) {
    const auto env_path = find_env_file();
    spdlog::info("Loading env from: {}", env_path.string());
    at::env::loadIntoProcess(env_path);

    const auto cli = parse_harvest_args(argc, argv);
    const auto from_ts = at::time::parseIso8601(cli.from_date);
    if (!from_ts) {
        throw std::runtime_error("Invalid --from date: " + cli.from_date);
    }

    at::harvest::HarvesterConfig cfg{
        .data_lake_root       = cli.data_lake,
        .symbol               = cli.symbol,
        .bar_size_setting     = cli.bar_size,
        .history_start        = *from_ts,
        .chunk_duration       = cli.chunk_duration,
        .inter_request_sleep  = std::chrono::milliseconds(
            std::stoll(at::env::getOr("HARVEST_SLEEP_MS", "12000"))),
        .pacing_retry_sleep   = std::chrono::milliseconds(
            std::stoll(at::env::getOr("HARVEST_PACING_SLEEP_MS", "60000"))),
    };

    at::ibkr::Session session(load_ibkr_config());
    session.connect();
    const int total = at::harvest::run(session, cfg);
    session.disconnect();

    spdlog::info("harvest: finished with {} bars appended", total);
    return total > 0 ? 0 : 1;
}
#endif

}  // namespace

int main(int argc, char** argv) {
    at::log::init("debug");

    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 0;
        }
        const std::string_view cmd = argv[1];
        if (cmd == "--help" || cmd == "-h") {
            print_usage(argv[0]);
            return 0;
        }
#if defined(ENABLE_IBKR)
        if (cmd == "--ibkr-check") {
            return run_ibkr_check();
        }
        if (cmd == "--harvest-test") {
            const std::string_view symbol =
                (argc >= 3) ? std::string_view{argv[2]} : std::string_view{"AAPL"};
            return run_harvest_test(symbol);
        }
        if (cmd == "--harvest") {
            return run_harvest(argc, argv);
        }
#else
        if (cmd == "--ibkr-check" || cmd == "--harvest-test" || cmd == "--harvest") {
            std::cerr << "dev_main built without ENABLE_IBKR; reconfigure with -DENABLE_IBKR=ON\n";
            return 2;
        }
#endif
        print_usage(argv[0]);
        return 2;
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}
