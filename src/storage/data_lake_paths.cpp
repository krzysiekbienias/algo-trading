#include "storage/data_lake_paths.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace at::storage {

namespace {

std::string formatMonth(int month) {
    if (month < 1 || month > 12) {
        throw std::invalid_argument("data_lake path month must be in [1, 12]");
    }
    if (month < 10) {
        return "0" + std::to_string(month);
    }
    return std::to_string(month);
}

}  // namespace

std::string canonicalSymbol(std::string_view symbol) {
    std::string out;
    out.reserve(symbol.size());
    for (const char ch : symbol) {
        if (ch == '.') {
            continue;
        }
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    if (out.empty()) {
        throw std::invalid_argument("canonicalSymbol: empty symbol");
    }
    return out;
}

std::filesystem::path symbolDir(const std::filesystem::path& root,
                                std::string_view symbol) {
    return root / canonicalSymbol(symbol);
}

std::filesystem::path pathFor(const std::filesystem::path& root,
                              std::string_view symbol,
                              int year,
                              int month) {
    if (year < 1970) {
        throw std::invalid_argument("data_lake path year must be >= 1970");
    }
    const auto canon = canonicalSymbol(symbol);
    const auto filename =
        canon + "_" + std::to_string(year) + "_" + formatMonth(month) + ".parquet";
    return symbolDir(root, symbol) / filename;
}

std::filesystem::path ensureSymbolDir(const std::filesystem::path& root,
                                      std::string_view symbol) {
    const auto dir = symbolDir(root, symbol);
    std::filesystem::create_directories(dir);
    return dir;
}

}  // namespace at::storage
