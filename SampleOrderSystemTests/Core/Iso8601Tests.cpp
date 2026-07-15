#include <gtest/gtest.h>

#include "Core/Iso8601.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace {

// Builds a UTC TimePoint from calendar components, independent of the code
// under test (uses <chrono> calendar types directly, the same primitives
// Iso8601.cpp is required to use -- this is deliberately not a call into
// TimePointToIso8601/ParseIso8601 itself).
std::chrono::system_clock::time_point MakeUtc(int year, unsigned month, unsigned day,
                                               int hour, int minute, int second) {
    using namespace std::chrono;
    sys_days days = year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{static_cast<unsigned>(day)}};
    return time_point_cast<system_clock::duration>(
        days + hours{hour} + minutes{minute} + seconds{second});
}

}  // namespace

TEST(Iso8601Test, TimePointToIso8601FormatsAKnownValue) {
    const auto tp = MakeUtc(2026, 7, 15, 10, 30, 0);
    EXPECT_EQ(TimePointToIso8601(tp), "2026-07-15T10:30:00Z");
}

TEST(Iso8601Test, TimePointToIso8601ZeroPadsSingleDigitComponents) {
    const auto tp = MakeUtc(2026, 1, 2, 3, 4, 5);
    EXPECT_EQ(TimePointToIso8601(tp), "2026-01-02T03:04:05Z");
}

TEST(Iso8601Test, TimePointToIso8601TruncatesSubSecondPrecisionInsteadOfRounding) {
    const auto whole = MakeUtc(2026, 7, 15, 10, 30, 0);
    const auto withMillis = whole + std::chrono::milliseconds(999);

    EXPECT_EQ(TimePointToIso8601(withMillis), "2026-07-15T10:30:00Z");
}

TEST(Iso8601Test, TimePointToIso8601IsUtcCorrectRegardlessOfLocalTimezoneRegressionTest) {
    // 23:30 UTC would roll to the next calendar day in UTC+1 or later if a
    // localtime-based implementation were used by mistake.
    const auto tp = MakeUtc(2026, 7, 15, 23, 30, 0);
    EXPECT_EQ(TimePointToIso8601(tp), "2026-07-15T23:30:00Z");
}

TEST(Iso8601Test, ParseIso8601RoundTripsThroughTimePointToIso8601ForRepresentativeValues) {
    const std::chrono::system_clock::time_point samples[] = {
        MakeUtc(2026, 7, 15, 10, 30, 0),
        MakeUtc(2024, 1, 1, 0, 0, 0),
        MakeUtc(1999, 12, 31, 23, 59, 59),
        MakeUtc(2000, 2, 29, 12, 0, 0),  // leap day
    };

    for (const auto& tp : samples) {
        EXPECT_EQ(ParseIso8601(TimePointToIso8601(tp)), tp);
    }
}

TEST(Iso8601Test, TimePointToIso8601RoundTripsThroughParseIso8601ForRepresentativeStrings) {
    const std::string samples[] = {
        "2026-07-15T10:30:00Z",
        "2024-01-01T00:00:00Z",
        "1999-12-31T23:59:59Z",
        "2000-02-29T12:00:00Z",
    };

    for (const auto& s : samples) {
        EXPECT_EQ(TimePointToIso8601(ParseIso8601(s)), s);
    }
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_TooShort) {
    EXPECT_THROW(ParseIso8601("2026-07-15T10:30:0Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_TooLong) {
    EXPECT_THROW(ParseIso8601("2026-07-15T10:30:00.000Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_MissingTSeparatorSpaceInstead) {
    EXPECT_THROW(ParseIso8601("2026-07-15 10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_MissingTrailingZOffsetInstead) {
    EXPECT_THROW(ParseIso8601("2026-07-15T10:30:00+00:00"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_NonDigitCharacterWhereADigitIsRequired) {
    EXPECT_THROW(ParseIso8601("202X-07-15T10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Month00) {
    EXPECT_THROW(ParseIso8601("2026-00-15T10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Month13) {
    EXPECT_THROW(ParseIso8601("2026-13-15T10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Day00) {
    EXPECT_THROW(ParseIso8601("2026-07-00T10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Day32) {
    EXPECT_THROW(ParseIso8601("2026-07-32T10:30:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_DayThatDoesNotExistInThatMonthFeb30) {
    EXPECT_THROW(ParseIso8601("2026-02-30T00:00:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Hour24) {
    EXPECT_THROW(ParseIso8601("2026-07-15T24:00:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Minute60) {
    EXPECT_THROW(ParseIso8601("2026-07-15T10:60:00Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_Second60) {
    EXPECT_THROW(ParseIso8601("2026-07-15T10:30:60Z"), std::invalid_argument);
}

TEST(Iso8601Test, ParseIso8601RejectsMalformedStrings_EmptyString) {
    EXPECT_THROW(ParseIso8601(""), std::invalid_argument);
}
