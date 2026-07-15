#pragma once
#include <chrono>
#include <string>


// Formats tp as a UTC ISO-8601 string with whole-second precision and a literal "Z" suffix,
// e.g. "2026-07-15T10:30:00Z" -- always exactly 20 characters, always zero-padded
// (single-digit month/day/hour/minute/second get a leading zero), never a UTC offset other
// than "Z", never fractional seconds. Any sub-second component in tp is truncated (floored,
// not rounded) and discarded.
std::string TimePointToIso8601(std::chrono::system_clock::time_point tp);

// Parses a string that must exactly match the "YYYY-MM-DDTHH:MM:SSZ" grammar produced by
// TimePointToIso8601 (fixed length 20, '-' at positions 4/7, 'T' at 10, ':' at 13/16, 'Z' at 19,
// all other positions ASCII digits). Throws std::invalid_argument (message includes the
// offending input) if the length/separators/digit-ness don't match, or if the numeric value is
// calendar-invalid (month outside 1-12, day outside 1-31, or a day that doesn't exist for that
// month/year, e.g. Feb 30; hour/minute/second are still validated against 0-23/0-59/0-59).
std::chrono::system_clock::time_point ParseIso8601(const std::string& text);
