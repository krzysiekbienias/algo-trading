#include "util/env.hpp"

#include <fmt/format.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

namespace at::env {

namespace {

// Trim ASCII whitespace from both ends.
std::string trim(std::string s) {
    auto is_ws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t b = 0;
    while (b < s.size() && is_ws(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && is_ws(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Strip surrounding matching single/double quotes (single pair only).
std::string unquote(std::string s) {
    if (s.size() >= 2) {
        const char f = s.front();
        const char b = s.back();
        if ((f == '"' && b == '"') || (f == '\'' && b == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

}  // namespace

EnvMap loadFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error(fmt::format("env: cannot open {}", path.string()));
    }

    EnvMap out;
    std::string line;
    size_t lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // Allow leading "export " for shell compatibility.
        if (trimmed.starts_with("export ")) {
            trimmed = trim(trimmed.substr(7));
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error(
                fmt::format("env: malformed line {} in {}: '{}'", lineno, path.string(), trimmed));
        }
        std::string key = trim(trimmed.substr(0, eq));
        std::string val = unquote(trim(trimmed.substr(eq + 1)));
        if (key.empty()) {
            throw std::runtime_error(
                fmt::format("env: empty key on line {} in {}", lineno, path.string()));
        }
        out.emplace(std::move(key), std::move(val));
    }
    return out;
}

EnvMap loadIntoProcess(const std::filesystem::path& path) {
    EnvMap m = loadFile(path);
    for (const auto& [k, v] : m) {
        // overwrite=0: OS-provided env always wins (CI/docker friendly).
        ::setenv(k.c_str(), v.c_str(), /*overwrite=*/0);
    }
    return m;
}

std::string getOr(std::string_view key, std::string_view default_value) {
    const std::string k(key);
    const char* v = std::getenv(k.c_str());
    if (v == nullptr || *v == '\0') return std::string(default_value);
    return std::string(v);
}

std::optional<std::string> get(std::string_view key) {
    const std::string k(key);
    const char* v = std::getenv(k.c_str());
    if (v == nullptr || *v == '\0') return std::nullopt;
    return std::string(v);
}

std::string require(std::string_view key) {
    auto v = get(key);
    if (!v) {
        throw std::runtime_error(fmt::format("env: required variable '{}' is not set", key));
    }
    return *v;
}

}  // namespace at::env
