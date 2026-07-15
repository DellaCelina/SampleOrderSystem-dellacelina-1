#include <gtest/gtest.h>

#include "Core/SystemClock.h"
#include "FakeClock.h"

#include <chrono>

TEST(ClockTest, TestHarnessIsWiredUp) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(ClockTest, SystemClockNowReturnsAPlausibleCurrentTime) {
    SystemClock clock;

    const auto before = std::chrono::system_clock::now();
    const auto observed = clock.Now();
    const auto after = std::chrono::system_clock::now();

    // Not the default-constructed / epoch time_point (catches a broken stub
    // that always returns time_point{}).
    EXPECT_NE(observed, std::chrono::system_clock::time_point{});

    // Within a generous tolerance of "real now" on either side (avoids
    // flakiness from the two now() calls straddling observed by a tick).
    const auto tolerance = std::chrono::seconds(1);
    EXPECT_GE(observed, before - tolerance);
    EXPECT_LE(observed, after + tolerance);
}

TEST(ClockTest, FakeClockDefaultConstructsToTheFixedDeterministicEpoch) {
    FakeClock clock;

    const auto expectedEpoch =
        std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}}; // 2024-01-01T00:00:00 UTC

    EXPECT_EQ(clock.Now(), expectedEpoch);
}

TEST(ClockTest, FakeClockConstructedWithAnExplicitStartTimeReflectsItExactly) {
    const auto start = std::chrono::system_clock::time_point{std::chrono::seconds{1700000000}};
    FakeClock clock(start);

    EXPECT_EQ(clock.Now(), start);
}

TEST(ClockTest, FakeClockSetTimeSetsAndNowReflectsItExactly) {
    FakeClock clock;
    const auto target = std::chrono::system_clock::time_point{std::chrono::seconds{1800000000}};

    clock.SetTime(target);

    EXPECT_EQ(clock.Now(), target);
}

TEST(ClockTest, FakeClockAdvanceMovesTimeForwardByTheGivenDuration_HourAlignedDuration) {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(std::chrono::hours(2));
    EXPECT_EQ(clock.Now(), start + std::chrono::hours(2));
}

TEST(ClockTest, FakeClockAdvanceMovesTimeForwardByTheGivenDuration_NonHourAlignedDuration) {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(std::chrono::minutes(90));
    EXPECT_EQ(clock.Now(), start + std::chrono::minutes(90));
}

TEST(ClockTest, FakeClockAdvanceWithANegativeDurationMovesTimeBackward) {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(-std::chrono::hours(3));

    EXPECT_EQ(clock.Now(), start - std::chrono::hours(3));
}

TEST(ClockTest, FakeClockAdvanceCalledMultipleTimesAccumulates) {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(std::chrono::hours(1));
    clock.Advance(std::chrono::minutes(30));

    EXPECT_EQ(clock.Now(), start + std::chrono::hours(1) + std::chrono::minutes(30));
}

TEST(ClockTest, FakeClockSetToExactlyATargetTimePointMatchesPreciselyNoFudge) {
    FakeClock clock;
    const auto target = std::chrono::system_clock::time_point{std::chrono::seconds{1704153600}}; // 2024-01-02T00:00:00 UTC

    clock.SetTime(target);

    // Exact equality, not >= with slack -- boundary "has the deadline
    // passed" logic in later phases depends on this being precise.
    EXPECT_EQ(clock.Now(), target);
    EXPECT_FALSE(clock.Now() != target);
}
