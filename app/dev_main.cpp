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
#include <vector>

#include "backfill/engine.hpp"
#include "util/env.hpp"
#include "util/log.hpp"
#include "util/time.hpp"
#include "xtb/client.hpp"
#include "xtb/types.hpp"

namespace {

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " --backfill [options]\n"
              << "       " << argv0 << " --help\n"
              << "\n"
              << "Backfill options:\n"
              << "  --backfill           Run BackfillEngine against XTB demo.\n"
              << "  --symbol SYMBOL      Instrument, e.g. EURUSD (repeatable).\n"
              << "  --period PERIOD      M1, M5, M15, M30, H1, H4, D1, W1, MN1.\n"
              << "  --out PATH           Parquet output path per symbol.\n"
              << "  --from DATE          History start (ISO-8601, e.g. 2024-01-01).\n"
              << "                       Default: 24h ago for M1/M5, 2 years for others.\n"
              << "\n"
              << "Requires .env with XTB_USER_ID and XTB_PASSWORD (demo account).\n"
              << "\n"
              << "Example:\n"
              << "  " << argv0 << " --backfill --symbol EURUSD --period H1 \\\n"
              << "    --out data/EURUSD_H1.parquet --from 2024-01-01\n";
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

at::xtb::Period parse_period(std::string_view s) {
    if (s == "M1")  return at::xtb::Period::M1;
    if (s == "M5")  return at::xtb::Period::M5;
    if (s == "M15") return at::xtb::Period::M15;
    if (s == "M30") return at::xtb::Period::M30;
    if (s == "H1")  return at::xtb::Period::H1;
    if (s == "H4")  return at::xtb::Period::H4;
    if (s == "D1")  return at::xtb::Period::D1;
    if (s == "W1")  return at::xtb::Period::W1;
    if (s == "MN1") return at::xtb::Period::MN1;
    throw std::runtime_error("Unknown period: " + std::string(s));
}

at::time::Timestamp default_history_start(at::xtb::Period period) {
    const auto now = at::time::now();
    using namespace std::chrono;
    switch (period) {
        case at::xtb::Period::M1:
        case at::xtb::Period::M5:
            return now - hours(24);
        default:
            return now - hours(24 * 365 * 2);
    }
}

at::xtb::ClientConfig load_client_config() {
    at::xtb::ClientConfig cfg;
    cfg.endpoint = at::env::getOr("XTB_ENDPOINT", "wss://ws.xapi.pro/demo");
    cfg.throttle = std::chrono::milliseconds(
        std::stoll(at::env::getOr("XTB_THROTTLE_MS", "250")));
    return cfg;
}

at::xtb::Credentials load_credentials() {
    return at::xtb::Credentials{
        .user_id  = at::env::require("XTB_USER_ID"),
        .password = at::env::require("XTB_PASSWORD"),
        .app_name = at::env::getOr("XTB_APP_NAME", "algo-trading-dev"),
    };
}

struct BackfillArgs {
    std::vector<at::backfill::BackfillEngine::SymbolConfig> symbols;
};

std::optional<std::string_view> flag_value(int argc, char** argv, int& i) {
    if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + argv[i]);
    }
    return std::string_view{argv[++i]};
}

BackfillArgs parse_backfill_args(int argc, char** argv) {
    BackfillArgs args;
    std::optional<std::string> symbol;
    std::optional<at::xtb::Period> period;
    std::optional<std::filesystem::path> out_path;
    std::optional<at::time::Timestamp> from_ts;

    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--help" || flag == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (flag == "--backfill") {
            continue;
        }
        if (flag == "--symbol") {
            symbol = std::string(*flag_value(argc, argv, i));
            continue;
        }
        if (flag == "--period") {
            period = parse_period(*flag_value(argc, argv, i));
            continue;
        }
        if (flag == "--out") {
            out_path = std::filesystem::path(*flag_value(argc, argv, i));
            continue;
        }
        if (flag == "--from") {
            const auto parsed = at::time::parseIso8601(*flag_value(argc, argv, i));
            if (!parsed) {
                throw std::runtime_error("Invalid --from date (use YYYY-MM-DD or ISO-8601 UTC)");
            }
            from_ts = *parsed;
            continue;
        }
        throw std::runtime_error("Unknown argument: " + std::string(flag));
    }

    if (!symbol || !period || !out_path) {
        throw std::runtime_error("--backfill requires --symbol, --period, and --out");
    }

    const auto history_start = from_ts.value_or(default_history_start(*period));

    std::filesystem::create_directories(out_path->parent_path());

    args.symbols.push_back(at::backfill::BackfillEngine::SymbolConfig{
        .symbol         = *symbol,
        .period         = *period,
        .output_path    = *out_path,
        .history_start  = history_start,
    });

    return args;
}

int run_backfill(int argc, char** argv) {
    const auto env_path = find_env_file();
    spdlog::info("Loading env from: {}", env_path.string());
    at::env::loadIntoProcess(env_path);

    const auto args = parse_backfill_args(argc, argv);
    const auto creds = load_credentials();
    const auto cfg   = load_client_config();

    at::xtb::Client client(cfg);
    spdlog::info("Connecting to {} ...", cfg.endpoint);
    client.connect();
    client.login(creds);
    spdlog::info("Logged in as {}", creds.user_id);

    at::backfill::BackfillEngine engine({.symbols = args.symbols});
    const int total = engine.run(client);

    client.disconnect();
    spdlog::info("Backfill complete: {} bars written", total);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    at::log::init("debug");

    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::string_view{argv[1]} == "--backfill") {
            return run_backfill(argc, argv);
        }
        print_usage(argv[0]);
        return 2;
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}
