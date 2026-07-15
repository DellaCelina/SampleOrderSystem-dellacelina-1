#pragma once
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../Json/JsonParser.h"
#include "../Json/JsonValue.h"

enum class FieldType { String, Integer, Number, Boolean };

struct FieldSchema {
    std::string name;
    FieldType type = FieldType::String;
    bool required = true;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<double> exclusiveMin;
    std::optional<double> exclusiveMax;
    std::optional<std::string> pattern;                  // string fields only
    std::optional<std::vector<std::string>> enumValues;  // string fields only
    bool isIso8601Format = false;                         // string fields only
};

struct Schema {
    std::string tableName;
    std::vector<FieldSchema> fields;

    // Parses a schema document (already-parsed JsonValue, e.g. from
    // JsonParser::Parse(ReadFile("schema/sample.schema.json"))) into a Schema.
    // Throws std::runtime_error on:
    //   - missing "table" or "fields" keys
    //   - a field missing "name" or "type"
    //   - an unrecognized "type" or "format" value
    //   - "pattern"/"enum" present on a non-string field
    static Schema FromJson(const JsonValue& schemaDoc);
};

namespace detail_schema {

inline FieldType ParseFieldType(const std::string& typeStr) {
    if (typeStr == "string") return FieldType::String;
    if (typeStr == "integer") return FieldType::Integer;
    if (typeStr == "number") return FieldType::Number;
    if (typeStr == "boolean") return FieldType::Boolean;
    throw std::runtime_error("Schema::FromJson: unrecognized field type \"" + typeStr + "\"");
}

}  // namespace detail_schema

inline Schema Schema::FromJson(const JsonValue& schemaDoc) {
    if (!schemaDoc.IsObject()) {
        throw std::runtime_error("Schema::FromJson: expected a JSON object schema document");
    }
    if (!schemaDoc.Has("table")) {
        throw std::runtime_error("Schema::FromJson: missing required \"table\" key");
    }
    if (!schemaDoc.Has("fields")) {
        throw std::runtime_error("Schema::FromJson: missing required \"fields\" key");
    }

    Schema schema;
    schema.tableName = schemaDoc.Get("table").AsString();

    const JsonValue& fieldsJson = schemaDoc.Get("fields");
    if (!fieldsJson.IsArray()) {
        throw std::runtime_error("Schema::FromJson: \"fields\" must be an array");
    }

    for (const JsonValue& fieldJson : fieldsJson.AsArray()) {
        if (!fieldJson.IsObject()) {
            throw std::runtime_error("Schema::FromJson: field entry must be an object");
        }
        if (!fieldJson.Has("name")) {
            throw std::runtime_error("Schema::FromJson: field entry missing required \"name\"");
        }
        if (!fieldJson.Has("type")) {
            throw std::runtime_error("Schema::FromJson: field entry missing required \"type\"");
        }

        FieldSchema field;
        field.name = fieldJson.Get("name").AsString();
        field.type = detail_schema::ParseFieldType(fieldJson.Get("type").AsString());

        if (const JsonValue* required = fieldJson.TryGet("required")) {
            field.required = required->AsBool();
        } else {
            field.required = true;
        }

        if (const JsonValue* min = fieldJson.TryGet("min")) {
            field.min = min->AsNumber();
        }
        if (const JsonValue* max = fieldJson.TryGet("max")) {
            field.max = max->AsNumber();
        }
        if (const JsonValue* exclusiveMin = fieldJson.TryGet("exclusiveMin")) {
            field.exclusiveMin = exclusiveMin->AsNumber();
        }
        if (const JsonValue* exclusiveMax = fieldJson.TryGet("exclusiveMax")) {
            field.exclusiveMax = exclusiveMax->AsNumber();
        }

        if (const JsonValue* pattern = fieldJson.TryGet("pattern")) {
            if (field.type != FieldType::String) {
                throw std::runtime_error("Schema::FromJson: \"pattern\" is only valid on string fields (\"" +
                                          field.name + "\")");
            }
            field.pattern = pattern->AsString();
        }

        if (const JsonValue* enumJson = fieldJson.TryGet("enum")) {
            if (field.type != FieldType::String) {
                throw std::runtime_error("Schema::FromJson: \"enum\" is only valid on string fields (\"" +
                                          field.name + "\")");
            }
            std::vector<std::string> values;
            for (const JsonValue& v : enumJson->AsArray()) {
                values.push_back(v.AsString());
            }
            field.enumValues = values;
        }

        if (const JsonValue* format = fieldJson.TryGet("format")) {
            const std::string& formatStr = format->AsString();
            if (formatStr != "iso8601") {
                throw std::runtime_error("Schema::FromJson: unrecognized format \"" + formatStr + "\"");
            }
            field.isIso8601Format = true;
        }

        schema.fields.push_back(field);
    }

    return schema;
}

// Reads `path`, parses it as JSON, and builds a Schema from the resulting
// schema document. Throws std::runtime_error (file not found/readable) or
// JsonParseException/std::runtime_error (parse/schema errors), per the
// underlying JsonParser::Parse / Schema::FromJson calls.
inline Schema LoadSchemaFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("LoadSchemaFromFile: could not open file \"" + path + "\"");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    JsonValue doc = JsonParser::Parse(buffer.str());
    return Schema::FromJson(doc);
}
