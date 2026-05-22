#include "ibkr/forex_pair.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace at::ibkr {

namespace {

std::string toUpper(std::string_view s) {
    std::string out(s);
    for (char& ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

}  // namespace

ForexPair parseForexPair(std::string_view symbol) {
    if (symbol.empty()) {
        throw std::invalid_argument("parseForexPair: empty symbol");
    }

    const auto dot = symbol.find('.');
    if (dot != std::string_view::npos) {
        if (dot == 0 || dot + 1 >= symbol.size()) {
            throw std::invalid_argument("parseForexPair: invalid dotted pair: " +
                                        std::string(symbol));
        }
        return ForexPair{
            .base  = toUpper(symbol.substr(0, dot)),
            .quote = toUpper(symbol.substr(dot + 1)),
        };
    }

    const std::string compact = toUpper(symbol);
    if (compact.size() != 6) {
        throw std::invalid_argument(
            "parseForexPair: expected EUR.USD or 6-letter pair (e.g. EURUSD), got: " +
            compact);
    }

    return ForexPair{
        .base  = compact.substr(0, 3),
        .quote = compact.substr(3, 3),
    };
}

}  // namespace at::ibkr
