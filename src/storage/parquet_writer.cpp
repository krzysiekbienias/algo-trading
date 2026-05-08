#include "storage/parquet_writer.hpp"

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace at::storage {

// Step 4 fills these in. Right now we only need the file to compile/link
// against arrow + parquet so the build pipeline is fully exercised.

ParquetWriter::ParquetWriter(std::filesystem::path path) : path_(std::move(path)) {}

void ParquetWriter::write(const std::vector<at::xtb::Bar>& bars) const {
    if (bars.empty()) {
        return;
    }

    // Sort bars by timestamp ascending so the on-disk order is canonical.
    std::vector<at::xtb::Bar> sorted_bars = bars;
    std::sort(sorted_bars.begin(), sorted_bars.end(),
              [](const at::xtb::Bar& a, const at::xtb::Bar& b) {
                  return at::time::toEpochMs(a.timestamp) <
                         at::time::toEpochMs(b.timestamp);
              });

    // Timestamp column must match the schema field type, so use
    // TimestampBuilder (NOT Int64Builder) with the same logical type.
    auto ts_type = arrow::timestamp(arrow::TimeUnit::MILLI, "UTC");
    arrow::TimestampBuilder timestamp_builder(ts_type, arrow::default_memory_pool());
    arrow::DoubleBuilder open_builder, high_builder, low_builder, close_builder, volume_builder;

    for (const auto& bar : sorted_bars) {
        PARQUET_THROW_NOT_OK(timestamp_builder.Append(at::time::toEpochMs(bar.timestamp)));
        PARQUET_THROW_NOT_OK(open_builder.Append(bar.open));
        PARQUET_THROW_NOT_OK(high_builder.Append(bar.high));
        PARQUET_THROW_NOT_OK(low_builder.Append(bar.low));
        PARQUET_THROW_NOT_OK(close_builder.Append(bar.close));
        PARQUET_THROW_NOT_OK(volume_builder.Append(bar.volume));
    }

    std::shared_ptr<arrow::Array> timestamps_arr, open_arr, high_arr, low_arr, close_arr, volume_arr;
    PARQUET_THROW_NOT_OK(timestamp_builder.Finish(&timestamps_arr));
    PARQUET_THROW_NOT_OK(open_builder.Finish(&open_arr));
    PARQUET_THROW_NOT_OK(high_builder.Finish(&high_arr));
    PARQUET_THROW_NOT_OK(low_builder.Finish(&low_arr));
    PARQUET_THROW_NOT_OK(close_builder.Finish(&close_arr));
    PARQUET_THROW_NOT_OK(volume_builder.Finish(&volume_arr));

    auto schema = arrow::schema({
        arrow::field("timestamp", ts_type),
        arrow::field("open",   arrow::float64()),
        arrow::field("high",   arrow::float64()),
        arrow::field("low",    arrow::float64()),
        arrow::field("close",  arrow::float64()),
        arrow::field("volume", arrow::float64()),
    });

    auto table = arrow::Table::Make(
        schema,
        {timestamps_arr, open_arr, high_arr, low_arr, close_arr, volume_arr});

    // Open the destination file. Default mode truncates, which is what we
    // want since write() overwrites by contract. Use the OutputStream
    // interface type so WriteTable accepts it directly.
    std::shared_ptr<arrow::io::OutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(
        outfile,
        arrow::io::FileOutputStream::Open(path_.string()));

    PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(
        *table, arrow::default_memory_pool(), outfile,
        static_cast<int64_t>(sorted_bars.size())));
}

void ParquetWriter::append(const std::vector<at::xtb::Bar>& bars) const {
    if (bars.empty()) {
        return;
    }

    // Read whatever is already on disk (empty vector if file doesn't exist yet).
    auto existing = readAll();

    // Merge into a std::map keyed by epoch-ms so that:
    //   * duplicates are deduplicated automatically (last insert wins)
    //   * entries stay sorted by timestamp
    // Old bars go in first, new bars go in second — new bars overwrite old on
    // duplicate timestamps ("newer wins" contract from the header).
    std::map<int64_t, at::xtb::Bar> merged;
    for (const auto& b : existing) {
        merged[at::time::toEpochMs(b.timestamp)] = b;
    }
    for (const auto& b : bars) {
        merged[at::time::toEpochMs(b.timestamp)] = b;
    }

    // Flatten back into a vector (map iteration is already sorted by key).
    std::vector<at::xtb::Bar> merged_bars;
    merged_bars.reserve(merged.size());
    for (auto& [ts, bar] : merged) {
        merged_bars.push_back(std::move(bar));
    }

    // Delegate the actual Parquet write to write(), which handles Arrow
    // builders, schema, and file I/O.
    write(merged_bars);
}

std::optional<at::time::Timestamp> ParquetWriter::readLastTimestamp() const {
    auto bars=readAll();
    if(bars.empty()) return std::nullopt;
    return bars.back().timestamp;
}

std::vector<at::xtb::Bar> ParquetWriter::readAll() const {
    // Step 1: file does not exist → no bars.
    if (!std::filesystem::exists(path_)) {
        return {};
    }

    // Step 2: open the file for reading. Mirror image of FileOutputStream.
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(
        infile,
        arrow::io::ReadableFile::Open(path_.string(), arrow::default_memory_pool()));

    // Step 3: build a parquet reader on top of that file. In Arrow 16+ the
    // free function OpenFile() returns Result<unique_ptr<FileReader>>, so we
    // use PARQUET_ASSIGN_OR_THROW to unwrap it.
    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_ASSIGN_OR_THROW(
        reader,
        parquet::arrow::OpenFile(infile, arrow::default_memory_pool()));

    // Step 4: read the entire file into a single arrow::Table (mirror of
    // parquet::arrow::WriteTable from write()).
    std::shared_ptr<arrow::Table> table;
    PARQUET_ASSIGN_OR_THROW(table, reader->ReadTable()); 

    // Step 5: schema sanity check. Catches "wrong file format" early with
    // a meaningful error instead of a confusing cast crash later.
    if (table->num_columns() != 6) {
        throw std::runtime_error(
            "ParquetWriter::readAll: unexpected number of columns in " +
            path_.string());
    }

    // Step 6: pull out typed arrays for each column. Table::column(i) returns
    // a ChunkedArray; for files we wrote ourselves there is exactly one chunk.
    auto get_double = [&](int idx) {
        return std::static_pointer_cast<arrow::DoubleArray>(
            table->column(idx)->chunk(0));
    };
    auto timestamps = std::static_pointer_cast<arrow::TimestampArray>(
        table->column(0)->chunk(0));
    auto opens   = get_double(1);
    auto highs   = get_double(2);
    auto lows    = get_double(3);
    auto closes  = get_double(4);
    auto volumes = get_double(5);

    // Step 7: rebuild Bar structs row-by-row. reserve() avoids re-allocations.
    const int64_t n = timestamps->length();
    std::vector<at::xtb::Bar> result;
    result.reserve(static_cast<size_t>(n));

    for (int64_t i = 0; i < n; ++i) {
        result.push_back(at::xtb::Bar{
            at::time::fromEpochMs(timestamps->Value(i)),
            opens->Value(i),
            highs->Value(i),
            lows->Value(i),
            closes->Value(i),
            volumes->Value(i),
        });
    }

    return result;
}

}  // namespace at::storage
