#include "storage/parquet_writer.hpp"

#include <arrow/api.h>
#include <parquet/arrow/writer.h>

#include <stdexcept>
#include <utility>

namespace at::storage {

// Step 4 fills these in. Right now we only need the file to compile/link
// against arrow + parquet so the build pipeline is fully exercised.

ParquetWriter::ParquetWriter(std::filesystem::path path) : path_(std::move(path)) {}

void ParquetWriter::write(const std::vector<at::xtb::Bar>&) const {
    throw std::runtime_error("storage::ParquetWriter::write() not implemented yet (Step 4)");
}

void ParquetWriter::append(const std::vector<at::xtb::Bar>&) const {
    throw std::runtime_error("storage::ParquetWriter::append() not implemented yet (Step 4)");
}

std::optional<at::time::Timestamp> ParquetWriter::readLastTimestamp() const {
    return std::nullopt;
}

std::vector<at::xtb::Bar> ParquetWriter::readAll() const {
    return {};
}

}  // namespace at::storage
