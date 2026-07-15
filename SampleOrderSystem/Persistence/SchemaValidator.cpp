#include "SchemaValidator.h"

#include <cmath>
#include <regex>

namespace {

std::string TypeName(FieldType type) {
    switch (type) {
        case FieldType::String: return "string";
        case FieldType::Integer: return "integer";
        case FieldType::Number: return "number";
        case FieldType::Boolean: return "boolean";
    }
    return "unknown";
}

std::string JsonTypeName(JsonType type) {
    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Bool: return "boolean";
        case JsonType::Number: return "number";
        case JsonType::String: return "string";
        case JsonType::Array: return "array";
        case JsonType::Object: return "object";
    }
    return "unknown";
}

bool IsValidIso8601(const std::string& value) {
    static const std::regex kShape(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    if (!std::regex_match(value, kShape)) {
        return false;
    }

    int month = std::stoi(value.substr(5, 2));
    int day = std::stoi(value.substr(8, 2));
    int hour = std::stoi(value.substr(11, 2));
    int minute = std::stoi(value.substr(14, 2));
    int second = std::stoi(value.substr(17, 2));

    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if (hour > 23) return false;
    if (minute > 59) return false;
    if (second > 59) return false;

    return true;
}

ValidationError MakeFieldError(size_t recordIndex, const std::string& fieldName, const std::string& detail) {
    return ValidationError{"record[" + std::to_string(recordIndex) + "]." + fieldName + ": " + detail, recordIndex,
                            fieldName};
}

std::optional<ValidationError> ValidateField(const JsonValue& record, const FieldSchema& field, size_t recordIndex) {
    const JsonValue* value = record.TryGet(field.name);
    if (value == nullptr) {
        if (field.required) {
            return MakeFieldError(recordIndex, field.name, "missing required field");
        }
        return std::nullopt;
    }

    switch (field.type) {
        case FieldType::String: {
            if (!value->IsString()) {
                return MakeFieldError(recordIndex, field.name, "expected string, got " + JsonTypeName(value->Type()));
            }
            const std::string& str = value->AsString();

            if (field.pattern.has_value()) {
                std::regex re(*field.pattern);
                if (!std::regex_match(str, re)) {
                    return MakeFieldError(recordIndex, field.name,
                                           "value \"" + str + "\" does not match pattern \"" + *field.pattern + "\"");
                }
            }

            if (field.enumValues.has_value()) {
                bool found = false;
                for (const std::string& allowed : *field.enumValues) {
                    if (allowed == str) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return MakeFieldError(recordIndex, field.name,
                                           "value \"" + str + "\" is not one of the allowed values");
                }
            }

            if (field.isIso8601Format) {
                if (!IsValidIso8601(str)) {
                    return MakeFieldError(recordIndex, field.name,
                                           "value \"" + str + "\" is not a valid ISO-8601 timestamp");
                }
            }
            break;
        }
        case FieldType::Boolean: {
            if (!value->IsBool()) {
                return MakeFieldError(recordIndex, field.name,
                                       "expected boolean, got " + JsonTypeName(value->Type()));
            }
            break;
        }
        case FieldType::Integer:
        case FieldType::Number: {
            if (!value->IsNumber()) {
                return MakeFieldError(recordIndex, field.name,
                                       "expected " + TypeName(field.type) + ", got " + JsonTypeName(value->Type()));
            }
            double num = value->AsNumber();

            if (field.type == FieldType::Integer && std::floor(num) != num) {
                return MakeFieldError(recordIndex, field.name, "expected integer, got non-integral number");
            }

            if (field.min.has_value() && num < *field.min) {
                return MakeFieldError(recordIndex, field.name, "value below minimum");
            }
            if (field.max.has_value() && num > *field.max) {
                return MakeFieldError(recordIndex, field.name, "value above maximum");
            }
            if (field.exclusiveMin.has_value() && num <= *field.exclusiveMin) {
                return MakeFieldError(recordIndex, field.name, "value must be greater than exclusive minimum");
            }
            if (field.exclusiveMax.has_value() && num >= *field.exclusiveMax) {
                return MakeFieldError(recordIndex, field.name, "value must be less than exclusive maximum");
            }
            break;
        }
    }

    return std::nullopt;
}

}  // namespace

std::optional<ValidationError> Validate(const JsonValue& data, const Schema& schema) {
    if (data.Type() != JsonType::Array) {
        return ValidationError{"expected top-level JSON array", std::nullopt, std::nullopt};
    }

    const std::vector<JsonValue>& records = data.AsArray();
    for (size_t i = 0; i < records.size(); ++i) {
        const JsonValue& record = records[i];
        if (record.Type() != JsonType::Object) {
            return ValidationError{"record[" + std::to_string(i) + "]: expected a JSON object", i, std::nullopt};
        }

        for (const FieldSchema& field : schema.fields) {
            std::optional<ValidationError> error = ValidateField(record, field, i);
            if (error.has_value()) {
                return error;
            }
        }
    }

    return std::nullopt;
}
