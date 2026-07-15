#pragma once
#include <chrono>

// Abstraction over wall-clock time so that any component reasoning about
// elapsed time or "has this deadline passed" (production queue completion
// sweeps, order timestamps, etc.) can be driven by a fake clock in tests
// instead of the real system clock. Every Service/Repository that needs
// "now" takes an IClock& (not a concrete clock, not a free function) so
// that tests can advance time deterministically without sleeping.
class IClock {
public:
    virtual ~IClock() = default;

    // Returns the current time point, using the same clock type
    // (std::chrono::system_clock) that persisted timestamps in JSON files
    // are serialized against, so time_point values read from disk and
    // values returned by this interface are directly comparable.
    virtual std::chrono::system_clock::time_point Now() const = 0;
};
