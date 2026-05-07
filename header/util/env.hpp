#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace at::env {

// Parsed contents of a .env file. Order is not preserved, comments stripped.
using EnvMap = std::unordered_map<std::string, std::string>;

// Load KEY=VALUE pairs from a .env-style file. Lines starting with '#' and
// blank lines are ignored. Quoted values (' or ") are unquoted. Throws
// std::runtime_error on I/O errors or malformed lines (with line number).
//
// Does NOT export to the process environment — use loadIntoProcess() for
// that. Keeping the two concerns separate makes testing easier.
EnvMap loadFile(const std::filesystem::path& path);

// Same as loadFile() but also exports each key to the current process via
// setenv() (overwrite=false: existing OS env wins, matching docker/12-factor
// conventions).
EnvMap loadIntoProcess(const std::filesystem::path& path);

// Get an env var with a default. Reads OS env first.
std::string getOr(std::string_view key, std::string_view default_value);

// Required env var. Throws std::runtime_error with a clear message if absent
// or empty. Use for credentials.
std::string require(std::string_view key);

// Optional env var. Returns std::nullopt if unset or empty.
std::optional<std::string> get(std::string_view key);

}  // namespace at::env
