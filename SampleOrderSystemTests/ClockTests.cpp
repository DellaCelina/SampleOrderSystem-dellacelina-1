#include "catch_amalgamated.hpp"

#include "Core/SystemClock.h"
#include "FakeClock.h"

#include <chrono>

TEST_CASE("test harness is wired up", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("SystemClock::Now returns a plausible current time", "[clock]") {
    SystemClock clock;

    const auto before = std::chrono::system_clock::now();
    const auto observed = clock.Now();
    const auto after = std::chrono::system_clock::now();

    // Not the default-constructed / epoch time_point (catches a broken stub
    // that always returns time_point{}).
    REQUIRE(observed != std::chrono::system_clock::time_point{});

    // Within a generous tolerance of "real now" on either side (avoids
    // flakiness from the two now() calls straddling observed by a tick).
    const auto tolerance = std::chrono::seconds(1);
    REQUIRE(observed >= before - tolerance);
    REQUIRE(observed <= after + tolerance);
}

TEST_CASE("FakeClock default-constructs to the fixed deterministic epoch", "[clock][fake-clock]") {
    FakeClock clock;

    const auto expectedEpoch =
        std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}}; // 2024-01-01T00:00:00 UTC

    REQUIRE(clock.Now() == expectedEpoch);
}

TEST_CASE("FakeClock constructed with an explicit start time reflects it exactly", "[clock][fake-clock]") {
    const auto start = std::chrono::system_clock::time_point{std::chrono::seconds{1700000000}};
    FakeClock clock(start);

    REQUIRE(clock.Now() == start);
}

TEST_CASE("FakeClock::SetTime sets and Now reflects it exactly", "[clock][fake-clock]") {
    FakeClock clock;
    const auto target = std::chrono::system_clock::time_point{std::chrono::seconds{1800000000}};

    clock.SetTime(target);

    REQUIRE(clock.Now() == target);
}

TEST_CASE("FakeClock::Advance moves time forward by the given duration", "[clock][fake-clock]") {
    FakeClock clock;
    const auto start = clock.Now();

    SECTION("hour-aligned duration") {
        clock.Advance(std::chrono::hours(2));
        REQUIRE(clock.Now() == start + std::chrono::hours(2));
    }

    SECTION("non-hour-aligned duration (catches truncation bugs)") {
        clock.Advance(std::chrono::minutes(90));
        REQUIRE(clock.Now() == start + std::chrono::minutes(90));
    }
}

TEST_CASE("FakeClock::Advance with a negative duration moves time backward", "[clock][fake-clock]") {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(-std::chrono::hours(3));

    REQUIRE(clock.Now() == start - std::chrono::hours(3));
}

TEST_CASE("FakeClock::Advance called multiple times accumulates", "[clock][fake-clock]") {
    FakeClock clock;
    const auto start = clock.Now();

    clock.Advance(std::chrono::hours(1));
    clock.Advance(std::chrono::minutes(30));

    REQUIRE(clock.Now() == start + std::chrono::hours(1) + std::chrono::minutes(30));
}

TEST_CASE("FakeClock set to exactly a target time point matches precisely, no fudge", "[clock][fake-clock]") {
    FakeClock clock;
    const auto target = std::chrono::system_clock::time_point{std::chrono::seconds{1704153600}}; // 2024-01-02T00:00:00 UTC

    clock.SetTime(target);

    // Exact equality, not >= with slack -- boundary "has the deadline
    // passed" logic in later phases depends on this being precise.
    REQUIRE(clock.Now() == target);
    REQUIRE_FALSE(clock.Now() != target);
}
