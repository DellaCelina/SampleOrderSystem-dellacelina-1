#include "JsonValue.h"

namespace {
const char* TypeName(JsonType type) {
    switch (type) {
        case JsonType::Null: return "Null";
        case JsonType::Bool: return "Bool";
        case JsonType::Number: return "Number";
        case JsonType::String: return "String";
        case JsonType::Array: return "Array";
        case JsonType::Object: return "Object";
    }
    return "Unknown";
}

[[noreturn]] void ThrowTypeMismatch(JsonType expected, JsonType actual) {
    throw JsonTypeException(std::string("Expected JsonValue of type ") + TypeName(expected) +
                             " but was " + TypeName(actual));
}
} // namespace

JsonValue::JsonValue() : m_value(std::monostate{}) {}

JsonValue::JsonValue(std::nullptr_t) : m_value(std::monostate{}) {}

JsonValue::JsonValue(bool value) : m_value(value) {}

JsonValue::JsonValue(double value) : m_value(value) {}

JsonValue::JsonValue(int value) : m_value(static_cast<double>(value)) {}

JsonValue::JsonValue(const std::string& value) : m_value(value) {}

JsonValue::JsonValue(const char* value) : m_value(std::string(value)) {}

JsonValue JsonValue::MakeNull() { return JsonValue(); }

JsonValue JsonValue::MakeBool(bool value) { return JsonValue(value); }

JsonValue JsonValue::MakeNumber(double value) { return JsonValue(value); }

JsonValue JsonValue::MakeString(std::string value) { return JsonValue(std::move(value)); }

JsonValue JsonValue::MakeArray() {
    JsonValue value;
    value.m_value = std::vector<JsonValue>();
    return value;
}

JsonValue JsonValue::MakeArray(std::vector<JsonValue> elements) {
    JsonValue value;
    value.m_value = std::move(elements);
    return value;
}

JsonValue JsonValue::MakeObject() {
    JsonValue value;
    value.m_value = ObjectEntries();
    return value;
}

JsonType JsonValue::Type() const {
    switch (m_value.index()) {
        case 0: return JsonType::Null;
        case 1: return JsonType::Bool;
        case 2: return JsonType::Number;
        case 3: return JsonType::String;
        case 4: return JsonType::Array;
        case 5: return JsonType::Object;
    }
    return JsonType::Null;
}

bool JsonValue::IsNull() const { return Type() == JsonType::Null; }
bool JsonValue::IsBool() const { return Type() == JsonType::Bool; }
bool JsonValue::IsNumber() const { return Type() == JsonType::Number; }
bool JsonValue::IsString() const { return Type() == JsonType::String; }
bool JsonValue::IsArray() const { return Type() == JsonType::Array; }
bool JsonValue::IsObject() const { return Type() == JsonType::Object; }

bool JsonValue::AsBool() const {
    if (!IsBool()) ThrowTypeMismatch(JsonType::Bool, Type());
    return std::get<bool>(m_value);
}

double JsonValue::AsNumber() const {
    if (!IsNumber()) ThrowTypeMismatch(JsonType::Number, Type());
    return std::get<double>(m_value);
}

const std::string& JsonValue::AsString() const {
    if (!IsString()) ThrowTypeMismatch(JsonType::String, Type());
    return std::get<std::string>(m_value);
}

const std::vector<JsonValue>& JsonValue::AsArray() const {
    if (!IsArray()) ThrowTypeMismatch(JsonType::Array, Type());
    return std::get<std::vector<JsonValue>>(m_value);
}

const JsonValue::ObjectEntries& JsonValue::AsObject() const {
    if (!IsObject()) ThrowTypeMismatch(JsonType::Object, Type());
    return std::get<ObjectEntries>(m_value);
}

std::vector<JsonValue>& JsonValue::AsArray() {
    if (!IsArray()) ThrowTypeMismatch(JsonType::Array, Type());
    return std::get<std::vector<JsonValue>>(m_value);
}

JsonValue::ObjectEntries& JsonValue::AsObject() {
    if (!IsObject()) ThrowTypeMismatch(JsonType::Object, Type());
    return std::get<ObjectEntries>(m_value);
}

void JsonValue::Push(JsonValue value) {
    if (!IsArray()) ThrowTypeMismatch(JsonType::Array, Type());
    std::get<std::vector<JsonValue>>(m_value).push_back(std::move(value));
}

void JsonValue::Set(const std::string& key, JsonValue value) {
    if (!IsObject()) ThrowTypeMismatch(JsonType::Object, Type());
    ObjectEntries& entries = std::get<ObjectEntries>(m_value);
    for (auto& entry : entries) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    entries.emplace_back(key, std::move(value));
}

bool JsonValue::Has(const std::string& key) const {
    if (!IsObject()) return false;
    const ObjectEntries& entries = std::get<ObjectEntries>(m_value);
    for (const auto& entry : entries) {
        if (entry.first == key) return true;
    }
    return false;
}

const JsonValue* JsonValue::TryGet(const std::string& key) const {
    if (!IsObject()) return nullptr;
    const ObjectEntries& entries = std::get<ObjectEntries>(m_value);
    for (const auto& entry : entries) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const JsonValue& JsonValue::Get(const std::string& key) const {
    const JsonValue* found = TryGet(key);
    if (found == nullptr) {
        throw std::out_of_range("JsonValue::Get: no such key: " + key);
    }
    return *found;
}

bool JsonValue::operator==(const JsonValue& other) const {
    return m_value == other.m_value;
}
