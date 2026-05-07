#pragma once

#include <string>
#include <vector>

#include "util/time.hpp"
#include "xtb/client.hpp"
#include "xtb/types.hpp"

namespace at::xtb::api {

// High-level XTB API wrappers built on top of Client::call(). Each function
// performs ONE network round-trip and returns typed data.
//
// Lives in `at::xtb::api` (a child of `at::xtb`) so the call site reads
// `at::xtb::api::getServerTime(client)` — visually separating the transport
// layer (`at::xtb::Client`) from the high-level command wrappers.

// `getServerTime` — useful as a sanity-check / heartbeat.
at::time::Timestamp getServerTime(Client& client);

// `getChartRangeRequest` — historical OHLCV bars in [start, end] for a single
// symbol. Prices are returned in human units (already divided by 10^digits)
// and timestamps are decoded from XTB's `ctm` (epoch ms, UTC) into our
// canonical chrono type.
//
// XTB caveat: depending on the symbol class (forex vs equity vs index) the
// broker enforces different historical limits. The backfill engine (Step 5)
// handles chunking; this function just executes one request.
std::vector<Bar> getChartRange(Client& client,
                               const std::string& symbol,
                               Period period,
                               at::time::Timestamp start,
                               at::time::Timestamp end);

}  // namespace at::xtb::api
