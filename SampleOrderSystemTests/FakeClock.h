#pragma once
#include "Core/IClock.h"
#include <chrono>

// Test double for IClock whose time is advanced explicitly by the test,
// never by real elapsed wall-clock time. Lets tests express "the
// production queue entry's completion time has now passed" without
// sleeping or depending on real-time timing flakiness.
class FakeClock final : public IClock {
public:
    // Starts at an arbitrary but fixed, deterministic epoch so tests don't
    // depend on "today" - chosen as 2024-01-01T00:00:00 UTC for readability
    // in failure messages. Tests that care about a specific start time
    // should call SetTime explicitly rather than relying on this default.
    FakeClock();
    explicit FakeClock(std::chrono::system_clock::time_point start);

    std::chrono::system_clock::time_point Now() const override;

    // Sets the clock to an absolute time point.
    void SetTime(std::chrono::system_clock::time_point time);

    // Advances the clock forward by the given duration. Passing a negative
    // duration is allowed (moves time backward) since nothing in this
    // class enforces monotonicity - tests that need "time never goes
    // backward" as a system invariant should test that invariant in the
    // component under test, not in the fake itself.
    void Advance(std::chrono::system_clock::duration delta);

private:
    std::chrono::system_clock::time_point m_time;
};

inline FakeClock::FakeClock()
    : m_time(std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}}) {}

inline FakeClock::FakeClock(std::chrono::system_clock::time_point start)
    : m_time(start) {}

inline std::chrono::system_clock::time_point FakeClock::Now() const {
    return m_time;
}

inline void FakeClock::SetTime(std::chrono::system_clock::time_point time) {
    m_time = time;
}

inline void FakeClock::Advance(std::chrono::system_clock::duration delta) {
    m_time += delta;
}
