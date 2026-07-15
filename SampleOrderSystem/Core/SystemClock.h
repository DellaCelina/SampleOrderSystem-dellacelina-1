#pragma once
#include "IClock.h"

// Production IClock implementation backed by the real OS wall clock.
// Stateless; safe to construct as a single shared instance (or one per
// call site) since it holds no members.
class SystemClock final : public IClock {
public:
    std::chrono::system_clock::time_point Now() const override;
};
