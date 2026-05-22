#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace at::storage {

// Path conventions for the local OHLCV data lake:
//
//   {root}/{CANONICAL}/{CANONICAL}_{YYYY}_{MM}.parquet
//
// Example: data_lake/AAPL/AAPL_2026_01.parquet
//
// CANONICAL is an uppercase filesystem-safe symbol (dots removed), e.g.
// TWS "EUR.USD" → "EURUSD".

// Maps broker/TWS symbols to directory and file prefix (e.g. "EUR.USD" → "EURUSD").
std::string canonicalSymbol(std::string_view symbol);

// Directory for one instrument: {root}/{CANONICAL}/.
std::filesystem::path symbolDir(const std::filesystem::path& root,
                                std::string_view symbol);

// Monthly shard path. Month must be in [1, 12].
std::filesystem::path pathFor(const std::filesystem::path& root,
                             std::string_view symbol,
                             int year,
                             int month);

// Creates {root}/{CANONICAL}/ if missing. Returns the symbol directory path.
std::filesystem::path ensureSymbolDir(const std::filesystem::path& root,
                                      std::string_view symbol);

}  // namespace at::storage
