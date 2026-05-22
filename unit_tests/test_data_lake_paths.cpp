#include <gtest/gtest.h>

#include "storage/data_lake_paths.hpp"

#include <stdexcept>
#include <string>

using at::storage::canonicalSymbol;
using at::storage::ensureSymbolDir;
using at::storage::pathFor;
using at::storage::symbolDir;

TEST(DataLakePaths, CanonicalSymbolStripsDotsAndUppercases) {
    EXPECT_EQ(canonicalSymbol("EUR.USD"), "EURUSD");
    EXPECT_EQ(canonicalSymbol("eur.usd"), "EURUSD");
    EXPECT_EQ(canonicalSymbol("AAPL"), "AAPL");
}

TEST(DataLakePaths, CanonicalSymbolRejectsEmpty) {
    EXPECT_THROW(canonicalSymbol(""), std::invalid_argument);
}

TEST(DataLakePaths, SymbolDir) {
    const std::filesystem::path root = "/data_lake";
    EXPECT_EQ(symbolDir(root, "AAPL"), root / "AAPL");
    EXPECT_EQ(symbolDir(root, "EUR.USD"), root / "EURUSD");
}

TEST(DataLakePaths, PathForMonthlyShard) {
    const std::filesystem::path root = "data_lake";
    EXPECT_EQ(pathFor(root, "AAPL", 2026, 1),
              std::filesystem::path("data_lake/AAPL/AAPL_2026_01.parquet"));
    EXPECT_EQ(pathFor(root, "NVDA", 2026, 12),
              std::filesystem::path("data_lake/NVDA/NVDA_2026_12.parquet"));
    EXPECT_EQ(pathFor(root, "EUR.USD", 2026, 2),
              std::filesystem::path("data_lake/EURUSD/EURUSD_2026_02.parquet"));
}

TEST(DataLakePaths, PathForRejectsInvalidMonth) {
    EXPECT_THROW(pathFor("data_lake", "AAPL", 2026, 0), std::invalid_argument);
    EXPECT_THROW(pathFor("data_lake", "AAPL", 2026, 13), std::invalid_argument);
}

TEST(DataLakePaths, EnsureSymbolDirCreatesDirectory) {
    const auto tmp = std::filesystem::temp_directory_path() / "at_data_lake_paths_test";
    std::filesystem::remove_all(tmp);

    const auto dir = ensureSymbolDir(tmp, "EUR.USD");
    EXPECT_TRUE(std::filesystem::is_directory(dir));
    EXPECT_EQ(dir, tmp / "EURUSD");

    std::filesystem::remove_all(tmp);
}
