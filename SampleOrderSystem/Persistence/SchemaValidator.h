#pragma once
#include <optional>
#include <string>

#include "../Json/JsonValue.h"
#include "Schema.h"

struct ValidationError {
    std::string message;                // human-readable
    std::optional<size_t> recordIndex;  // which array element failed, if applicable
    std::optional<std::string> fieldName;

    bool operator==(const ValidationError& other) const {
        return message == other.message && recordIndex == other.recordIndex && fieldName == other.fieldName;
    }
};

// Validates that `data` is a JSON array, and that every element is a JSON
// object satisfying every field in `schema` (type, required-ness, numeric
// bounds, pattern, enum, iso8601-shape). Returns std::nullopt on success,
// or the FIRST validation error encountered (fail-fast).
std::optional<ValidationError> Validate(const JsonValue& data, const Schema& schema);
