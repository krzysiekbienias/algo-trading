// Production CLI for the algo-trading pipeline.
//
// Step 1 scope: bootstrap-only — verify that all dependencies link, that
// logging works, and that we can resolve the .env file. Real backfill logic
// arrives in Steps 2-6.

#include <arrow/api.h>
#include <ixwebsocket/IXWebSocketVersion.h>
#include <nlohmann/json.hpp>
#include <parquet/api/writer.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include "util/env.hpp"
#include "util/log.hpp"

namespace {

constexpr std::string_view kVersion = "0.1.0";

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--help] [--version] [--check]\n"
              << "\n"
              << "Options:\n"
              << "  --help       Show this message and exit.\n"
              << "  --version    Print version and exit.\n"
              << "  --check      Run dependency / env smoke check.\n"
              << "\n"
              << "Backfill flags (--symbol/--from/--to/--out) land in Step 5.\n";
}

void print_versions() {
    std::cout << "algo-trading " << kVersion << "\n"
              << "  Arrow C++       : " << arrow::GetBuildInfo().version_string << "\n"
              << "  IXWebSocket     : " << IX_WEBSOCKET_VERSION << "\n"
              << "  nlohmann_json   : " << NLOHMANN_JSON_VERSION_MAJOR << "."
              << NLOHMANN_JSON_VERSION_MINOR << "." << NLOHMANN_JSON_VERSION_PATCH << "\n"
              << "  spdlog          : " << SPDLOG_VER_MAJOR << "." << SPDLOG_VER_MINOR << "."
              << SPDLOG_VER_PATCH << "\n";
}

int run_check() {
    spdlog::info("Smoke check starting...");

    // Resolve project root by walking up from the executable until we find
    // a .env or .env.example. Lets the user run ./build/app from any cwd.
    namespace fs = std::filesystem;
    fs::path here = fs::current_path();
    fs::path env_path;
    for (int depth = 0; depth < 6; ++depth) {
        if (fs::exists(here / ".env")) {
            env_path = here / ".env";
            break;
        }
        if (fs::exists(here / ".env.example") && env_path.empty()) {
            env_path = here / ".env.example";
        }
        if (here.has_parent_path() && here != here.parent_path()) {
            here = here.parent_path();
        } else {
            break;
        }
    }

    if (env_path.empty()) {
        spdlog::warn("No .env or .env.example found in cwd or parents.");
    } else {
        spdlog::info("Loading env from: {}", env_path.string());
        const auto map = at::env::loadIntoProcess(env_path);
        spdlog::info("Loaded {} env entries.", map.size());
    }

    const auto user_id = at::env::get("XTB_USER_ID");
    if (user_id && !user_id->empty()) {
        spdlog::info("XTB_USER_ID is set ({} digits).", user_id->size());
    } else {
        spdlog::warn("XTB_USER_ID is not set yet (fill in .env before Step 2).");
    }

    spdlog::info("Smoke check OK.");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    at::log::init();

    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 0;
        }
        const std::string_view arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "--version" || arg == "-v") {
            print_versions();
            return 0;
        }
        if (arg == "--check") {
            return run_check();
        }
        std::cerr << "Unknown argument: " << arg << "\n";
        print_usage(argv[0]);
        return 2;
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
}
