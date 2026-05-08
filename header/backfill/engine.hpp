#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "storage/parquet_writer.hpp"
#include "util/time.hpp"
#include "xtb/api.hpp"
#include "xtb/client.hpp"
#include "xtb/types.hpp"

namespace at::backfill {

class BackfillEngine {
public:
    struct SymbolConfig {
        std::string            symbol;
        at::xtb::Period        period;
        std::filesystem::path  output_path;
        at::time::Timestamp    history_start;
    };

    struct Config {
        std::vector<SymbolConfig> symbols;
        int chunk_bars = 5000;
    };

    explicit BackfillEngine(Config config);

    // Run backfill for all symbols sequentially.
    // Returns total number of new bars written across all symbols.
    int run(at::xtb::Client& client) const;

private:
    // Backfill a single symbol. Called by run() for each entry in config_.symbols.
    int runOne(at::xtb::Client& client, const SymbolConfig& sym) const;

    Config config_;
};

}  // namespace at::backfill
