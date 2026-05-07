#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace at::time {

// Canonical timestamp type used everywhere in the codebase.
//
// Why this exact type?
//   * XTB's `ctm` field in `getChartRangeRequest` returns milliseconds since
//     Unix epoch in UTC. One-to-one match with `sys_time<milliseconds>`, so
//     parsing is a single explicit cast — no precision loss, no timezone
//     ambiguity.
//   * Apache Arrow's `timestamp[ms, UTC]` maps to the same underlying
//     int64 representation. Storage round-trips are lossless.
//   * Using a strong type (instead of raw int64) prevents the classic
//     "did you mean seconds or milliseconds?" bug at compile time.
//
// If we ever ingest tick-level data with sub-millisecond resolution, we
// promote this alias to `sys_time<microseconds>` (or ns) and update the
// Parquet schema in lockstep — the rest of the code uses the alias.
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

// Number of milliseconds since Unix epoch (UTC). Apache Arrow / Parquet
// store timestamps as int64 in this exact unit, so this is the wire form.
inline std::int64_t toEpochMs(Timestamp tp) noexcept {
    return tp.time_since_epoch().count();
}

// Inverse of toEpochMs(). XTB's `ctm` field plugs straight into here.
inline Timestamp fromEpochMs(std::int64_t ms) noexcept {
    return Timestamp{std::chrono::milliseconds{ms}};
}

// Convenience helpers for human-friendly horizons used by the backfill CLI.
inline Timestamp fromEpochSeconds(std::int64_t s) noexcept {
    return Timestamp{std::chrono::milliseconds{s * 1000}};
}

// Current wall-clock time, truncated to millisecond precision.
inline Timestamp now() noexcept {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
}

// ISO-8601 in UTC, e.g. "2026-05-06T17:42:13.123Z". Used in logs and CLI
// output. The trailing 'Z' makes the timezone explicit.
std::string formatIso8601(Timestamp tp);

// Parse an ISO-8601 / RFC-3339-style timestamp into our canonical type.
// Accepts the forms:
//   2026-05-06                         (date only -> 00:00:00 UTC)
//   2026-05-06T17:42:13Z
//   2026-05-06T17:42:13.123Z
//   2026-05-06 17:42:13                (legacy, assumed UTC)
//
// Returns std::nullopt on any parse failure (caller decides how to react).
std::optional<Timestamp> parseIso8601(std::string_view s);

}  // namespace at::time
