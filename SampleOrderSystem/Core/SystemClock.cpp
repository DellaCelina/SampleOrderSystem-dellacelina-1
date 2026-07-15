#include "SystemClock.h"

std::chrono::system_clock::time_point SystemClock::Now() const {
    return std::chrono::system_clock::now();
}
