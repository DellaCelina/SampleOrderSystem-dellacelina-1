#include "Iso8601.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

using namespace std::chrono;

std::string TimePointToIso8601(std::chrono::system_clock::time_point tp) {
    const auto whole = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    const sys_days days = std::chrono::floor<std::chrono::days>(whole);
    const year_month_day ymd{days};
    const std::chrono::seconds timeOfDay = whole - days;
    const hh_mm_ss<std::chrono::seconds> hms{timeOfDay};

    char buffer[21];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02lld:%02lld:%02lldZ",
                  static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()),
                  static_cast<long long>(hms.hours().count()),
                  static_cast<long long>(hms.minutes().count()),
                  static_cast<long long>(hms.seconds().count()));

    return std::string(buffer);
}

namespace {

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

int DigitsToInt(const std::string& text, std::size_t start, std::size_t count) {
    int value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        value = value * 10 + (text[start + i] - '0');
    }
    return value;
}

}  // namespace

std::chrono::system_clock::time_point ParseIso8601(const std::string& text) {
    const auto fail = [&text]() {
        throw std::invalid_argument("ParseIso8601: malformed ISO-8601 string: \"" + text + "\"");
    };

    if (text.size() != 20) {
        fail();
    }
    if (text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') {
        fail();
    }
    static const std::size_t digitPositions[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (std::size_t pos : digitPositions) {
        if (!IsDigit(text[pos])) {
            fail();
        }
    }

    const int year = DigitsToInt(text, 0, 4);
    const unsigned month = static_cast<unsigned>(DigitsToInt(text, 5, 2));
    const unsigned day = static_cast<unsigned>(DigitsToInt(text, 8, 2));
    const int hour = DigitsToInt(text, 11, 2);
    const int minute = DigitsToInt(text, 14, 2);
    const int second = DigitsToInt(text, 17, 2);

    const year_month_day ymd{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
    if (!ymd.ok()) {
        fail();
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        fail();
    }

    const sys_days days = ymd;
    const auto tp = days + std::chrono::hours{hour} + std::chrono::minutes{minute} +
                    std::chrono::seconds{second};
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp);
}
