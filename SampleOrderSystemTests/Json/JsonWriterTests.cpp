#include "catch_amalgamated.hpp"

#include "Json/JsonParser.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"

#include <string>

TEST_CASE("Write null/true/false", "[json][json-writer]") {
    REQUIRE(JsonWriter::Write(JsonValue()) == "null");
    REQUIRE(JsonWriter::Write(JsonValue(true)) == "true");
    REQUIRE(JsonWriter::Write(JsonValue(false)) == "false");
}

TEST_CASE("Write numbers with no gratuitous trailing .0", "[json][json-writer]") {
    REQUIRE(JsonWriter::Write(JsonValue(5)) == "5");
    REQUIRE(JsonWriter::Write(JsonValue(0.9)) == "0.9");
    REQUIRE(JsonWriter::Write(JsonValue(-3)) == "-3");
}

TEST_CASE("Write a simple string", "[json][json-writer]") {
    REQUIRE(JsonWriter::Write(JsonValue(std::string("hi"))) == "\"hi\"");
}

TEST_CASE("String escaping: quote, backslash, newline", "[json][json-writer]") {
    JsonValue value(std::string("a\"b\\c\n"));

    REQUIRE(JsonWriter::Write(value) == "\"a\\\"b\\\\c\\n\"");
}

TEST_CASE("Control character not covered by a named escape uses \\u00XX", "[json][json-writer]") {
    JsonValue value(std::string("\x01"));

    REQUIRE(JsonWriter::Write(value) == "\"\\u0001\"");
}

TEST_CASE("Write empty array and empty object", "[json][json-writer]") {
    REQUIRE(JsonWriter::Write(JsonValue::MakeArray()) == "[]");
    REQUIRE(JsonWriter::Write(JsonValue::MakeObject()) == "{}");
}

TEST_CASE("Write a small nested fixture with exact pretty-printed output", "[json][json-writer]") {
    JsonValue array = JsonValue::MakeArray();
    array.Push(JsonValue(1));
    array.Push(JsonValue(2));

    JsonValue object = JsonValue::MakeObject();
    object.Set("name", "sample");
    object.Set("count", 7);
    object.Set("items", array);

    const std::string expected =
        "{\n"
        "  \"name\": \"sample\",\n"
        "  \"count\": 7,\n"
        "  \"items\": [\n"
        "    1,\n"
        "    2\n"
        "  ]\n"
        "}";

    REQUIRE(JsonWriter::Write(object) == expected);
}

TEST_CASE("Object key order in output exactly matches insertion order", "[json][json-writer]") {
    JsonValue object = JsonValue::MakeObject();
    object.Set("zebra", 1);
    object.Set("apple", 2);

    const std::string output = JsonWriter::Write(object);
    const size_t zebraPos = output.find("zebra");
    const size_t applePos = output.find("apple");

    REQUIRE(zebraPos != std::string::npos);
    REQUIRE(applePos != std::string::npos);
    REQUIRE(zebraPos < applePos);
}

TEST_CASE("Round trip via structural equality for builder-constructed trees", "[json][json-writer]") {
    JsonValue simpleObject = JsonValue::MakeObject();
    simpleObject.Set("a", 1);
    simpleObject.Set("b", "two");
    simpleObject.Set("c", true);

    JsonValue nested = JsonValue::MakeObject();
    JsonValue innerArray = JsonValue::MakeArray();
    innerArray.Push(JsonValue(1.5));
    innerArray.Push(JsonValue::MakeObject());
    nested.Set("items", innerArray);
    nested.Set("empty", JsonValue::MakeArray());

    for (const JsonValue& value : {simpleObject, nested, JsonValue::MakeObject(), JsonValue::MakeArray()}) {
        JsonValue roundTripped = JsonParser::Parse(JsonWriter::Write(value));
        REQUIRE(roundTripped == value);
    }
}

TEST_CASE("Round trip via text is idempotent from the second parse onward", "[json][json-writer]") {
    const std::string inputs[] = {
        R"({"a":1,"b":[1,2,3],"c":{"nested":true,"note":null}})",
        R"([])",
        R"({})",
        R"([{"x":1},{"y":2}])",
    };

    for (const std::string& text : inputs) {
        JsonValue firstParse = JsonParser::Parse(text);
        std::string written = JsonWriter::Write(firstParse);
        JsonValue secondParse = JsonParser::Parse(written);

        REQUIRE(secondParse == firstParse);
    }
}
