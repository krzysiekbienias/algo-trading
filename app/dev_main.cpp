// Scratchpad executable for ad-hoc experiments. Built only when
// ENABLE_DEV_MAIN=ON. Promote tested ideas into app/main.cpp when stable.

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "util/env.hpp"
#include "util/log.hpp"

#if defined(ENABLE_IBKR)
#include "ibkr/contract.hpp"
#include "ibkr/historical.hpp"
#include "ibkr/session.hpp"
#endif

namespace {

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " --ibkr-check\n"
              << "       " << argv0 << " --harvest-test [SYMBOL]\n"
              << "       " << argv0 << " --help\n"
              << "\n"
              << "IBKR TWS (requires TWS / IB Gateway with API enabled):\n"
              << "  --ibkr-check       Connect and call reqCurrentTime.\n"
              << "  --harvest-test     Fetch 1 day of 1-hour bars (default symbol: AAPL).\n"
              << "\n"
              << "Env (see .env.example): IBKR_HOST, IBKR_PORT, IBKR_CLIENT_ID\n"
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
#else
        if (cmd == "--ibkr-check" || cmd == "--harvest-test") {
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
