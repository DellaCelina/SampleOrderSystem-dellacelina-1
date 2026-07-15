#pragma once
#include <string>
#include "JsonValue.h"

class JsonWriter {
public:
    // Serializes `value` to a pretty-printed JSON string: 2-space indent per
    // nesting level, one member/element per line for non-empty
    // Object/Array, object key order exactly matching JsonValue's own
    // insertion order (never re-sorted), no trailing newline at the very
    // end of the returned string. See JsonWriter.cpp for the exact
    // per-JsonType formatting rules (number formatting, string escaping).
    static std::string Write(const JsonValue& value);
};
