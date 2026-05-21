#include "backfill/engine.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include "storage/parquet_writer.hpp"
#include "util/time.hpp"
#include "xtb/api.hpp"
#include "xtb/types.hpp"

namespace at::backfill {

BackfillEngine::BackfillEngine(Config config) : config_(std::move(config)) {}

int BackfillEngine::run(at::xtb::Client& client) const {
    int total = 0;
    for (const auto& sym : config_.symbols) {
        total += runOne(client, sym);
    }
    return total;
}

int BackfillEngine::runOne(at::xtb::Client& client, const SymbolConfig& sym) const {
    at::storage::ParquetWriter writer(sym.output_path);
    auto last_ts=writer.readLastTimestamp();
    auto period_duration = std::chrono::minutes(static_cast<int>(sym.period));
    at::time::Timestamp from;
    int total=0;
    at::time::Timestamp to_ts = at::time::now();
    if(last_ts.has_value()){
        from=*last_ts+period_duration;
    }
    else{
        from=sym.history_start;
    }
    while(true){
        auto bars= at::xtb::api::getChartRange(client, sym.symbol, sym.period, from,to_ts);
        if(bars.empty()) {
            break;
        }
        writer.append(bars);
        total+=bars.size();
        from=bars.back().timestamp + period_duration; // move starting point
        if(from>=to_ts) break;
    }
    return total;
    
}

}  // namespace at::backfill
