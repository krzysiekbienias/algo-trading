#include <gtest/gtest.h>

#include "util/time.hpp"

using namespace at::time;

// Known reference point: 2000-01-01T00:00:00.000Z = 946 684 800 000 ms
static constexpr std::int64_t kY2kMs = 946'684'800'000LL;

// ─── toEpochMs / fromEpochMs ─────────────────────────────────────────────────

TEST(Time, EpochMsRoundTrip) {
    const std::int64_t ms = 1'746'529'333'123LL;
    EXPECT_EQ(toEpochMs(fromEpochMs(ms)), ms);
}

TEST(Time, EpochMsZero) {
    EXPECT_EQ(toEpochMs(fromEpochMs(0)), 0);
}

TEST(Time, EpochMsNegative) {
    // Timestamps before Unix epoch should be representable.
    const std::int64_t ms = -1'000LL;
    EXPECT_EQ(toEpochMs(fromEpochMs(ms)), ms);
}

// ─── fromEpochSeconds ────────────────────────────────────────────────────────

TEST(Time, FromEpochSecondsZero) {
    EXPECT_EQ(toEpochMs(fromEpochSeconds(0)), 0);
}

TEST(Time, FromEpochSecondsConversion) {
    EXPECT_EQ(toEpochMs(fromEpochSeconds(1)), 1'000LL);
    EXPECT_EQ(toEpochMs(fromEpochSeconds(60)), 60'000LL);
}

// ─── formatIso8601 ───────────────────────────────────────────────────────────

TEST(Time, FormatIso8601KnownTimestamp) {
    EXPECT_EQ(formatIso8601(fromEpochMs(kY2kMs)), "2000-01-01T00:00:00.000Z");
}

TEST(Time, FormatIso8601WithMilliseconds) {
    EXPECT_EQ(formatIso8601(fromEpochMs(kY2kMs + 123)), "2000-01-01T00:00:00.123Z");
}

TEST(Time, FormatIso8601EndsWithZ) {
    const auto result = formatIso8601(fromEpochMs(kY2kMs));
    EXPECT_EQ(result.back(), 'Z');
}

// ─── parseIso8601 ────────────────────────────────────────────────────────────

TEST(Time, ParseIso8601DateOnly) {
    const auto result = parseIso8601("2000-01-01");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(toEpochMs(*result), kY2kMs);
}

TEST(Time, ParseIso8601WithZ) {
    const auto result = parseIso8601("2000-01-01T00:00:00Z");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(toEpochMs(*result), kY2kMs);
}

TEST(Time, ParseIso8601WithMilliseconds) {
    const auto result = parseIso8601("2000-01-01T00:00:00.123Z");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(toEpochMs(*result), kY2kMs + 123);
}

TEST(Time, ParseIso8601LegacySpaceSeparator) {
    const auto result = parseIso8601("2000-01-01 00:00:00");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(toEpochMs(*result), kY2kMs);
}

TEST(Time, ParseIso8601EmptyReturnsNullopt) {
    EXPECT_FALSE(parseIso8601("").has_value());
}

TEST(Time, ParseIso8601OnlyZReturnsNullopt) {
    EXPECT_FALSE(parseIso8601("Z").has_value());
}

TEST(Time, ParseIso8601GarbageReturnsNullopt) {
    EXPECT_FALSE(parseIso8601("not-a-date").has_value());
    EXPECT_FALSE(parseIso8601("2000/01/01").has_value());
}

// ─── Format → Parse round-trip ───────────────────────────────────────────────

TEST(Time, FormatParseRoundTrip) {
    const auto original = fromEpochMs(kY2kMs + 456);
    const auto parsed   = parseIso8601(formatIso8601(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(toEpochMs(*parsed), toEpochMs(original));
}

// ─── formatTwsHistoricalEndUtc ───────────────────────────────────────────────

TEST(Time, FormatTwsHistoricalEndUtcUsesExplicitUtcSeparator) {
    EXPECT_EQ(formatTwsHistoricalEndUtc(fromEpochMs(kY2kMs)), "20000101-00:00:00");
}

// ─── now() ───────────────────────────────────────────────────────────────────

TEST(Time, NowIsAfter2024) {
    // Sanity check: the clock is not stuck in the past.
    // 2024-01-01T00:00:00Z = 1 704 067 200 000 ms
    EXPECT_GT(toEpochMs(now()), 1'704'067'200'000LL);
}
