#include "util/time.hpp"

#include <fmt/chrono.h>

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace at::time {

std::string formatIso8601(Timestamp tp) {
    // {:%FT%T} on sys_time<milliseconds> renders as "2026-05-06T17:42:13.123"
    // with sub-second precision baked in by the chrono formatter — no manual
    // ms-extraction needed. We append the explicit 'Z' UTC marker.
    return fmt::format("{:%FT%T}Z", tp);
}

std::optional<Timestamp> parseIso8601(std::string_view s) {
    using namespace std::chrono;

    if (s.empty()) return std::nullopt;

    // Strip an optional 'Z' UTC marker (uppercase only — RFC-3339 strict).
    // We ALWAYS interpret times as UTC; the marker is optional purely for
    // input ergonomics.
    std::string buf(s);
    if (buf.back() == 'Z') {
        buf.pop_back();
    }
    // Defensive: input might have been just "Z", leaving an empty buf.
    if (buf.empty()) return std::nullopt;

    // chrono::parse needs a full datetime to fill a sys_time<ms>. For
    // date-only input ("2026-05-06") we synthesize the missing time portion
    // up-front rather than juggling sys_days as a fallback target.
    if (buf.size() == 10 && buf[4] == '-' && buf[7] == '-') {
        buf += "T00:00:00";
    }

    // Try the supported separators in priority order. chrono::parse handles
    // optional sub-second milliseconds inside %T natively (so ".123" works
    // out of the box).
    static constexpr const char* kFormats[] = {
        "%FT%T",
        "%F %T",
    };

    for (const char* fmt : kFormats) {
        Timestamp tp;
        std::istringstream is(buf);
        is >> parse(fmt, tp);
        if (!is.fail() && (is.eof() || is.peek() == std::char_traits<char>::eof())) {
            return tp;
        }
    }
    return std::nullopt;
}

}  // namespace at::time
