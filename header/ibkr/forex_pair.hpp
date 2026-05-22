#pragma once

#include <string>
#include <string_view>

namespace at::ibkr {

struct ForexPair {
    std::string base;
    std::string quote;
};

// Parses TWS-style FX symbols: "EUR.USD", "eur.usd", or "EURUSD" (6 letters).
ForexPair parseForexPair(std::string_view symbol);

}  // namespace at::ibkr
