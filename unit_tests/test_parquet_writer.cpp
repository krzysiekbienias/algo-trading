#include <gtest/gtest.h>

#include "storage/parquet_writer.hpp"
#include "util/time.hpp"
#include "xtb/types.hpp"

#include <filesystem>
#include <vector>

using at::storage::ParquetWriter;
using at::time::fromEpochMs;
using at::time::toEpochMs;
using at::xtb::Bar;

// Temporary directory for files created during tests.
// Each test gets a fresh file via SetUp() / TearDown().
static const std::filesystem::path kTmpDir =
    std::filesystem::temp_directory_path() / "at_parquet_tests";

// Helper: build a Bar with an explicit timestamp and OHLCV values.
static Bar makeBar(std::int64_t ts_ms,
                   double open, double high, double low,
                   double close, double volume) {
    return Bar{fromEpochMs(ts_ms), open, high, low, close, volume};
}

// Test fixture — creates a temp dir and a clean file path for every test.
class ParquetWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(kTmpDir);
        path_ = kTmpDir / "test.parquet";
        std::filesystem::remove(path_);
    }

    void TearDown() override {
        std::filesystem::remove(path_);
    }

    std::filesystem::path path_;
};

// ─── Constructor / path() ────────────────────────────────────────────────────

TEST_F(ParquetWriterTest, ConstructorSetsPath) {
    ParquetWriter w(path_);
    EXPECT_EQ(w.path(), path_);
}

// ─── Behaviour when file does not exist ─────────────────────────────────────

TEST_F(ParquetWriterTest, ReadLastTimestampReturnsNulloptWhenNoFile) {
    ParquetWriter w(path_);
    EXPECT_FALSE(w.readLastTimestamp().has_value());
}

TEST_F(ParquetWriterTest, ReadAllReturnsEmptyWhenNoFile) {
    ParquetWriter w(path_);
    EXPECT_TRUE(w.readAll().empty());
}

// ─── write() — Step 4 ────────────────────────────────────────────────────────
// Tests below are skipped until ParquetWriter::write() is implemented.
// Remove GTEST_SKIP() when Step 4 is done.

TEST_F(ParquetWriterTest, WriteAndReadAllRoundTrip) {
    
    const std::vector<Bar> bars = {
        makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0),
        makeBar(2'000'000, 1.15, 1.30, 1.10, 1.25, 200.0),
    };

    ParquetWriter w(path_);
    w.write(bars);

    const auto result = w.readAll();
    ASSERT_EQ(result.size(), 2u);

    EXPECT_EQ(toEpochMs(result[0].timestamp), 1'000'000);
    EXPECT_DOUBLE_EQ(result[0].open,   1.10);
    EXPECT_DOUBLE_EQ(result[0].high,   1.20);
    EXPECT_DOUBLE_EQ(result[0].low,    1.00);
    EXPECT_DOUBLE_EQ(result[0].close,  1.15);
    EXPECT_DOUBLE_EQ(result[0].volume, 100.0);

    EXPECT_EQ(toEpochMs(result[1].timestamp), 2'000'000);
}

TEST_F(ParquetWriterTest, WriteSortsByTimestamp) {
    
    // Bars passed in reverse order — must come out sorted ascending.
    const std::vector<Bar> bars = {
        makeBar(3'000'000, 1.30, 1.40, 1.20, 1.35, 300.0),
        makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0),
        makeBar(2'000'000, 1.20, 1.30, 1.10, 1.25, 200.0),
    };

    ParquetWriter w(path_);
    w.write(bars);

    const auto result = w.readAll();
    ASSERT_EQ(result.size(), 3u);
    EXPECT_LT(toEpochMs(result[0].timestamp), toEpochMs(result[1].timestamp));
    EXPECT_LT(toEpochMs(result[1].timestamp), toEpochMs(result[2].timestamp));
}

TEST_F(ParquetWriterTest, ReadLastTimestampAfterWrite) {
    
    // Bars intentionally out of order — readLastTimestamp must return the max.
    const std::vector<Bar> bars = {
        makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0),
        makeBar(3'000'000, 1.30, 1.40, 1.20, 1.35, 300.0),
        makeBar(2'000'000, 1.20, 1.30, 1.10, 1.25, 200.0),
    };

    ParquetWriter w(path_);
    w.write(bars);

    const auto last = w.readLastTimestamp();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(toEpochMs(*last), 3'000'000);
}

TEST_F(ParquetWriterTest, WriteEmptyBarsProducesNoFile) {
    
    ParquetWriter w(path_);
    w.write({});

    // Writing zero bars should be a no-op — no file created.
    EXPECT_FALSE(std::filesystem::exists(path_));
}

// ─── append() — Step 4 ───────────────────────────────────────────────────────

TEST_F(ParquetWriterTest, AppendCreatesFileIfMissing) {

    ParquetWriter w(path_);
    ASSERT_FALSE(std::filesystem::exists(path_));

    w.append({makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0)});

    EXPECT_TRUE(std::filesystem::exists(path_));
    ASSERT_EQ(w.readAll().size(), 1u);
}

TEST_F(ParquetWriterTest, AppendMergesWithExistingBars) {

    ParquetWriter w(path_);
    w.write({makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0)});
    w.append({makeBar(2'000'000, 1.20, 1.30, 1.10, 1.25, 200.0)});

    const auto result = w.readAll();
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(toEpochMs(result[0].timestamp), 1'000'000);
    EXPECT_EQ(toEpochMs(result[1].timestamp), 2'000'000);
}

TEST_F(ParquetWriterTest, AppendDeduplicatesNewerWins) {

    ParquetWriter w(path_);
    // Write a bar with open=1.1.
    w.write({makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0)});
    // Append a bar with the same timestamp but different values — newer wins.
    w.append({makeBar(1'000'000, 9.99, 9.99, 9.99, 9.99, 999.0)});

    const auto result = w.readAll();
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].open, 9.99);
}

TEST_F(ParquetWriterTest, AppendUpdatesLastTimestamp) {

    ParquetWriter w(path_);
    w.write({makeBar(1'000'000, 1.10, 1.20, 1.00, 1.15, 100.0)});
    w.append({makeBar(5'000'000, 1.50, 1.60, 1.40, 1.55, 500.0)});

    const auto last = w.readLastTimestamp();
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(toEpochMs(*last), 5'000'000);
}
