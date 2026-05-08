#include "backfill/engine.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <stdexcept>

namespace at::backfill {

BackfillEngine::BackfillEngine(Config config) : config_(std::move(config)) {}

int BackfillEngine::run(at::xtb::Client& client) const {
    // TODO: implement
}

int BackfillEngine::runOne(at::xtb::Client& client, const SymbolConfig& sym) const {
    // TODO: implement
}

}  // namespace at::backfill
