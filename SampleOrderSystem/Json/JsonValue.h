#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

enum class JsonType { Null, Bool, Number, String, Array, Object };

// Thrown by any accessor (AsBool/AsNumber/AsString/AsArray/AsObject) when the
// JsonValue's actual Type() does not match what the caller asked for.
// Deliberately a distinct type from JsonParseException (below): this one
// signals "the caller misused an already-successfully-parsed/constructed
// value," not "the input text was malformed" -- callers that want to
// distinguish "field is present but wrong type" from "field is missing"
// should check Type()/Has() first rather than relying on this exception for
// control flow (later phases, e.g. Sample::FromJson, are expected to do
// exactly that).
class JsonTypeException : public std::logic_error {
public:
    explicit JsonTypeException(const std::string& message) : std::logic_error(message) {}
};

class JsonValue {
public:
    using ObjectEntries = std::vector<std::pair<std::string, JsonValue>>;

    // -- Construction: every JsonValue starts life as one concrete variant. --
    JsonValue();                                  // Null
    JsonValue(std::nullptr_t);                     // Null (explicit convenience)
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(int value);                          // convenience: forwards to double
    JsonValue(const std::string& value);
    JsonValue(const char* value);

    // -- Static factories: preferred way to build Array/Object values, since --
    // -- there is no "vector<JsonValue> means Array" implicit constructor    --
    // -- (an implicit ctor from vector<JsonValue> would collide ambiguously  --
    // -- with wanting numbers/strings inside braced-init in some call sites; --
    // -- explicit factories avoid that ambiguity entirely).                 --
    static JsonValue MakeNull();
    static JsonValue MakeBool(bool value);
    static JsonValue MakeNumber(double value);
    static JsonValue MakeString(std::string value);
    static JsonValue MakeArray();                              // empty array
    static JsonValue MakeArray(std::vector<JsonValue> elements);
    static JsonValue MakeObject();                             // empty object

    JsonType Type() const;
    bool IsNull() const;
    bool IsBool() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsObject() const;

    // -- Read accessors. Each throws JsonTypeException if Type() doesn't --
    // -- match, naming both the expected and actual type in the message. --
    bool AsBool() const;
    double AsNumber() const;
    const std::string& AsString() const;
    const std::vector<JsonValue>& AsArray() const;
    const ObjectEntries& AsObject() const;

    // -- Mutating accessors, for building up Array/Object values in place --
    // -- after construction via MakeArray()/MakeObject() (used by every   --
    // -- later phase's ToJson()). Same throwing behavior as the const     --
    // -- overloads above if Type() doesn't match.                        --
    std::vector<JsonValue>& AsArray();
    ObjectEntries& AsObject();

    // Appends to an Array value. Throws JsonTypeException if this value is
    // not an Array (including if it's a default-constructed Null -- callers
    // must MakeArray() first, Push never silently converts Null-to-Array).
    void Push(JsonValue value);

    // Upserts a key on an Object value: if `key` already exists, its VALUE is
    // replaced in place at its ORIGINAL position (position never moves on
    // update); if `key` does not exist, a new (key, value) pair is appended
    // at the end. Throws JsonTypeException if this value is not an Object.
    void Set(const std::string& key, JsonValue value);

    // Object-only queries. Has() returns false (never throws) if this value
    // is not an Object at all, or if it is an Object without that key --
    // "not an object" and "object without the key" are deliberately not
    // distinguished by Has(), since every real caller's next step is the
    // same either way ("this field isn't available").
    bool Has(const std::string& key) const;

    // Returns a pointer to the value for `key`, or nullptr if absent (or if
    // this value is not an Object). Never throws -- the non-throwing
    // counterpart to Get(), for callers that want to branch on absence
    // without exception-based control flow (e.g. optional schema fields).
    const JsonValue* TryGet(const std::string& key) const;

    // Returns the value for `key`. Throws std::out_of_range (message
    // includes the key name) if this value is not an Object, or is an
    // Object without that key. Callers that need to distinguish "wrong
    // type" from "missing key" should check Type()/Has() first -- Get()
    // collapses both failure modes into the same exception type since, for
    // most call sites (a required field lookup), both are equally fatal.
    const JsonValue& Get(const std::string& key) const;

    // Structural (deep) equality. Object comparison is ORDER-SENSITIVE --
    // two Objects with the same keys/values in different insertion order
    // compare unequal. This is intentional: it matches the round-trip
    // determinism this type exists to provide (see design decision above),
    // and every equality check this system's tests actually need compares
    // a value against itself after a round trip, where order is preserved
    // by construction, not against an independently-authored reordering.
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    // Implementation detail, not part of the contract above: back this with
    // std::variant<std::monostate, bool, double, std::string,
    // std::vector<JsonValue>, ObjectEntries> (std::monostate for Null).
    // Nothing outside JsonValue.cpp should touch the variant directly.
    std::variant<std::monostate, bool, double, std::string,
                 std::vector<JsonValue>, ObjectEntries> m_value;
};
