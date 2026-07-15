#include "Console.h"

#include <windows.h>

namespace AnsiColor {
const char* const kRed = "\x1b[31m";
const char* const kGreen = "\x1b[32m";
const char* const kYellow = "\x1b[33m";
const char* const kBlue = "\x1b[34m";
const char* const kMagenta = "\x1b[35m";
const char* const kReset = "\x1b[0m";
}  // namespace AnsiColor

void EnableConsoleAnsiAndUtf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != INVALID_HANDLE_VALUE && out != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(out, &mode)) {
            SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

std::string ClearScreenSequence() {
    return "\x1b[2J\x1b[H";
}

std::string Colorize(const std::string& ansiCode, const std::string& text) {
    return ansiCode + text + AnsiColor::kReset;
}

std::string ColorizeStatus(OrderStatus status, const std::string& text) {
    switch (status) {
        case OrderStatus::Reserved:
            return Colorize(AnsiColor::kYellow, text);
        case OrderStatus::Confirmed:
            return Colorize(AnsiColor::kGreen, text);
        case OrderStatus::Producing:
            return Colorize(AnsiColor::kBlue, text);
        case OrderStatus::Released:
            return Colorize(AnsiColor::kMagenta, text);
        case OrderStatus::Rejected:
            return Colorize(AnsiColor::kRed, text);
        default:
            return text;
    }
}

std::string SeparatorLine() {
    return "============================================================\n";
}

std::string HeaderBlock() {
    return ClearScreenSequence() + "반도체 시료 생산주문관리 시스템\n" + SeparatorLine() + "\n";
}
