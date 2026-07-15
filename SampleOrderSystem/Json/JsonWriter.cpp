#include "JsonWriter.h"

#include <charconv>
#include <cstdio>

namespace {

void WriteIndent(std::string& out, int level) {
    out.append(static_cast<size_t>(level) * 2, ' ');
}

void WriteEscapedString(std::string& out, const std::string& value) {
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c <= 0x1F) {
                    char buffer[7];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

void WriteNumber(std::string& out, double value) {
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, result.ptr);
}

void WriteValue(std::string& out, const JsonValue& value, int level);

void WriteArray(std::string& out, const JsonValue& value, int level) {
    const std::vector<JsonValue>& elements = value.AsArray();
    if (elements.empty()) {
        out += "[]";
        return;
    }
    out += "[\n";
    for (size_t i = 0; i < elements.size(); ++i) {
        WriteIndent(out, level + 1);
        WriteValue(out, elements[i], level + 1);
        if (i + 1 < elements.size()) {
            out += ",";
        }
        out += "\n";
    }
    WriteIndent(out, level);
    out += "]";
}

void WriteObject(std::string& out, const JsonValue& value, int level) {
    const JsonValue::ObjectEntries& entries = value.AsObject();
    if (entries.empty()) {
        out += "{}";
        return;
    }
    out += "{\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        WriteIndent(out, level + 1);
        WriteEscapedString(out, entries[i].first);
        out += ": ";
        WriteValue(out, entries[i].second, level + 1);
        if (i + 1 < entries.size()) {
            out += ",";
        }
        out += "\n";
    }
    WriteIndent(out, level);
    out += "}";
}

void WriteValue(std::string& out, const JsonValue& value, int level) {
    switch (value.Type()) {
        case JsonType::Null: out += "null"; break;
        case JsonType::Bool: out += value.AsBool() ? "true" : "false"; break;
        case JsonType::Number: WriteNumber(out, value.AsNumber()); break;
        case JsonType::String: WriteEscapedString(out, value.AsString()); break;
        case JsonType::Array: WriteArray(out, value, level); break;
        case JsonType::Object: WriteObject(out, value, level); break;
    }
}

} // namespace

std::string JsonWriter::Write(const JsonValue& value) {
    std::string out;
    WriteValue(out, value, 0);
    return out;
}
