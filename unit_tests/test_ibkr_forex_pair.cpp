#include <gtest/gtest.h>

#include "ibkr/forex_pair.hpp"

using at::ibkr::parseForexPair;

TEST(ForexPair, ParseDottedSymbol) {
    const auto pair = parseForexPair("EUR.USD");
    EXPECT_EQ(pair.base, "EUR");
    EXPECT_EQ(pair.quote, "USD");
}

TEST(ForexPair, ParseCompactSymbol) {
    const auto pair = parseForexPair("eurusd");
    EXPECT_EQ(pair.base, "EUR");
    EXPECT_EQ(pair.quote, "USD");
}

TEST(ForexPair, RejectsInvalidCompact) {
    EXPECT_THROW(parseForexPair("EUR"), std::invalid_argument);
}
