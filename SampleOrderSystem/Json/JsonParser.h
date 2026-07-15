#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include "JsonValue.h"

// Thrown by JsonParser::Parse on any malformed input. Carries enough
// location info (1-based line/column of the character where parsing failed)
// that a caller reporting a load failure (e.g. JsonFileStore in phase-3)
// can produce a message naming both the file and roughly where in it the
// problem is, without JsonParser itself knowing about files.
class JsonParseException : public std::runtime_error {
public:
    JsonParseException(const std::string& message, size_t line, size_t column);

    size_t Line() const { return m_line; }     // 1-based
    size_t Column() const { return m_column; } // 1-based

private:
    size_t m_line;
    size_t m_column;
};

class JsonParser {
public:
    // Parses `text` as a single JSON document and returns the resulting
    // JsonValue. Throws JsonParseException on any malformed input (see
    // grammar and rejection list below). Leading/trailing ASCII whitespace
    // (space, tab, '\n', '\r') around the single top-level value is
    // tolerated; anything else after the top-level value ends is an error
    // ("trailing garbage").
    static JsonValue Parse(const std::string& text);
};
