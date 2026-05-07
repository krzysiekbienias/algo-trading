#include "xtb/api.hpp"

#include <stdexcept>

namespace at::xtb::api {

// Stubs to be implemented in Step 3 once the Client transport layer is alive.
// Kept here so that downstream modules (parquet writer, backfill engine) can
// compile-test against the signatures.

at::time::Timestamp getServerTime(Client&) {
    throw std::runtime_error("xtb::api::getServerTime() not implemented yet (Step 3)");
}

std::vector<Bar> getChartRange(Client&,
                               const std::string& /*symbol*/,
                               Period /*period*/,
                               at::time::Timestamp /*start*/,
                               at::time::Timestamp /*end*/) {
    throw std::runtime_error("xtb::api::getChartRange() not implemented yet (Step 3)");
}

}  // namespace at::xtb::api
