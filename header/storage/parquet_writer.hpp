#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "market/types.hpp"
#include "util/time.hpp"

namespace at::storage {

// One Parquet file per instrument, schema:
//   timestamp : timestamp[ms, UTC]
//   open      : double
//   high      : double
//   low       : double
//   close     : double
//   volume    : double
//
// We pick this schema (rather than e.g. float32) because:
//   * timestamps are explicit UTC ms — no ambiguity, plays nice with DuckDB,
//     pandas, polars, Spark
//   * doubles preserve full OHLCV precision for equities and FX
//
// Append-mode strategy: Parquet has no native append. We rewrite the file
// using a temp + rename for atomicity, after merging old + new bars and
// deduplicating on timestamp.

class ParquetWriter {
public:
    explicit ParquetWriter(std::filesystem::path path);

    // Atomically write `bars` to disk, OVERWRITING any existing file.
    // Bars are sorted by timestamp before write.
    void write(const std::vector<at::market::Bar>& bars) const;

    // Append `bars` to the existing file (or create it if missing).
    // Old + new are merged, deduplicated by timestamp (newer wins on tie),
    // sorted, then written atomically.
    void append(const std::vector<at::market::Bar>& bars) const;

    // Returns the maximum timestamp found in the file, or std::nullopt if
    // the file does not exist or contains no rows. Used for incremental
    // backfill resume: "continue from last_ts + 1 bar".
    std::optional<at::time::Timestamp> readLastTimestamp() const;

    // Read all bars from the file (mostly for tests / inspection).
    std::vector<at::market::Bar> readAll() const;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace at::storage
