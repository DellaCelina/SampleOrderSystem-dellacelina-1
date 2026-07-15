#pragma once
#include <string>

#include "../Models/Order.h"

// Small free-function helpers for making the console UI feel like navigating
// a browser: enabling ANSI/VT escape processing + UTF-8 on Windows once at
// startup, plus escape-sequence helpers Views/Controllers write directly into
// their existing injected std::ostream&. Deliberately NOT a class/interface
// (no IConsoleIO) -- this repo's Views/Controllers are simple stream-injected
// free functions/classes, and this is meant to slot into that shape, not
// replace it.

// Enables UTF-8 console I/O and VT100/ANSI escape sequence processing on
// Windows. Must be called once, at the very top of main(), before any
// repositories/views/controllers are constructed or any output is written.
// No-op-safe to call more than once.
void EnableConsoleAnsiAndUtf8();

// Returns the ANSI escape sequence that clears the whole screen and moves
// the cursor to the top-left corner ("home"), so a screen redraw replaces the
// prior screen instead of scrolling underneath it.
std::string ClearScreenSequence();

// Returns `text` wrapped in the ANSI SGR color code matching `status`
// (RESERVED=yellow, CONFIRMED=green, PRODUCING=blue, RELEASED=magenta,
// REJECTED=red), with a trailing reset code. Presentation only -- does not
// affect the plain OrderStatusToString() value used for persistence.
std::string ColorizeStatus(OrderStatus status, const std::string& text);

// Generic ANSI SGR wrap: `code` is applied before `text`, then reset.
std::string Colorize(const std::string& ansiCode, const std::string& text);

// Reusable ANSI SGR color codes for presentation-only badges outside the
// OrderStatus set (e.g. stock-level badges on the monitoring screen).
namespace AnsiColor {
extern const char* const kRed;
extern const char* const kGreen;
extern const char* const kYellow;
extern const char* const kBlue;
extern const char* const kMagenta;
extern const char* const kReset;
}  // namespace AnsiColor

// A single "==...==" separator line (with trailing newline) used to give
// every menu/list screen a bordered look.
std::string SeparatorLine();

// The persistent app title/header block (title line + separator + blank
// line), written at the top of every screen redraw.
std::string HeaderBlock();
