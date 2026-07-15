#include "catch_amalgamated.hpp"

#include "Json/JsonParser.h"
#include "Json/JsonValue.h"

#include <string>

TEST_CASE("Parse literals: true, false, null", "[json][json-parser]") {
    JsonValue trueValue = JsonParser::Parse("true");
    REQUIRE(trueValue.Type() == JsonType::Bool);
    REQUIRE(trueValue.AsBool() == true);

    JsonValue falseValue = JsonParser::Parse("false");
    REQUIRE(falseValue.Type() == JsonType::Bool);
    REQUIRE(falseValue.AsBool() == false);

    JsonValue nullValue = JsonParser::Parse("null");
    REQUIRE(nullValue.IsNull());
}

TEST_CASE("Parse numbers in various forms", "[json][json-parser]") {
    REQUIRE(JsonParser::Parse("42").AsNumber() == 42.0);
    REQUIRE(JsonParser::Parse("-7").AsNumber() == -7.0);
    REQUIRE(JsonParser::Parse("0").AsNumber() == 0.0);
    REQUIRE(JsonParser::Parse("3.14").AsNumber() == 3.14);
    REQUIRE(JsonParser::Parse("-0.5").AsNumber() == -0.5);
    REQUIRE(JsonParser::Parse("1e10").AsNumber() == 1e10);
    REQUIRE(JsonParser::Parse("1.5E-3").AsNumber() == 1.5E-3);
    REQUIRE(JsonParser::Parse("2e+5").AsNumber() == 2e+5);
}

TEST_CASE("Parse a string with every named escape", "[json][json-parser]") {
    JsonValue value = JsonParser::Parse("\"\\\"a\\\\b/c\\bd\\fe\\nf\\rg\\th\\\"\"");

    std::string expected = "\"a\\b/c\bd\fe\nf\rg\th\"";
    REQUIRE(value.AsString() == expected);
}

TEST_CASE("Parse a \\uXXXX escape for a BMP character", "[json][json-parser]") {
    JsonValue value = JsonParser::Parse("\"\\u00e9\"");

    REQUIRE(value.AsString() == "\xC3\xA9"); // UTF-8 encoding of U+00E9 (é)
}

TEST_CASE("Parse empty array and empty object", "[json][json-parser]") {
    JsonValue array = JsonParser::Parse("[]");
    REQUIRE(array.Type() == JsonType::Array);
    REQUIRE(array.AsArray().empty());

    JsonValue object = JsonParser::Parse("{}");
    REQUIRE(object.Type() == JsonType::Object);
    REQUIRE(object.AsObject().empty());
}

TEST_CASE("Parse a nested structure with heterogeneous leaf types", "[json][json-parser]") {
    const std::string text = R"([
        {"name": "a", "count": 1, "active": true, "note": null, "tags": ["x", "y"]},
        {"name": "b", "count": 2, "active": false, "note": null, "tags": []}
    ])";

    JsonValue root = JsonParser::Parse(text);
    REQUIRE(root.Type() == JsonType::Array);
    REQUIRE(root.AsArray().size() == 2);

    const JsonValue& first = root.AsArray()[0];
    REQUIRE(first.Get("name").AsString() == "a");
    REQUIRE(first.Get("count").AsNumber() == 1.0);
    REQUIRE(first.Get("active").AsBool() == true);
    REQUIRE(first.Get("note").IsNull());
    REQUIRE(first.Get("tags").AsArray().size() == 2);
    REQUIRE(first.Get("tags").AsArray()[0].AsString() == "x");
    REQUIRE(first.Get("tags").AsArray()[1].AsString() == "y");

    const JsonValue& second = root.AsArray()[1];
    REQUIRE(second.Get("name").AsString() == "b");
    REQUIRE(second.Get("active").AsBool() == false);
    REQUIRE(second.Get("tags").AsArray().empty());
}

TEST_CASE("Leading/trailing whitespace around a top-level value is tolerated", "[json][json-parser]") {
    JsonValue withWhitespace = JsonParser::Parse("  \n\t{ \"a\" : 1 }\n  ");
    JsonValue withoutWhitespace = JsonParser::Parse("{\"a\":1}");

    REQUIRE(withWhitespace == withoutWhitespace);
}

TEST_CASE("Duplicate keys in one object: last value wins, first position kept", "[json][json-parser]") {
    JsonValue value = JsonParser::Parse(R"({"a":1,"b":2,"a":3})");

    const auto& entries = value.AsObject();
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].first == "a");
    REQUIRE(entries[0].second.AsNumber() == 3.0);
    REQUIRE(entries[1].first == "b");
    REQUIRE(entries[1].second.AsNumber() == 2.0);
}

TEST_CASE("Malformed input throws JsonParseException", "[json][json-parser]") {
    SECTION("empty string") {
        REQUIRE_THROWS_AS(JsonParser::Parse(""), JsonParseException);
    }
    SECTION("unterminated object") {
        REQUIRE_THROWS_AS(JsonParser::Parse("{"), JsonParseException);
    }
    SECTION("unterminated array") {
        REQUIRE_THROWS_AS(JsonParser::Parse("["), JsonParseException);
    }
    SECTION("trailing garbage after a complete value") {
        REQUIRE_THROWS_AS(JsonParser::Parse("123 abc"), JsonParseException);
    }
    SECTION("missing colon") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"({"a" 1})"), JsonParseException);
    }
    SECTION("missing comma between array elements") {
        REQUIRE_THROWS_AS(JsonParser::Parse("[1 2]"), JsonParseException);
    }
    SECTION("missing comma between object members") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"({"a":1 "b":2})"), JsonParseException);
    }
    SECTION("unquoted key") {
        REQUIRE_THROWS_AS(JsonParser::Parse("{a:1}"), JsonParseException);
    }
    SECTION("single-quoted key/string") {
        REQUIRE_THROWS_AS(JsonParser::Parse("{'a':1}"), JsonParseException);
    }
    SECTION("trailing comma in array") {
        REQUIRE_THROWS_AS(JsonParser::Parse("[1,2,]"), JsonParseException);
    }
    SECTION("trailing comma in object") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"({"a":1,})"), JsonParseException);
    }
    SECTION("invalid escape sequence") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"("\q")"), JsonParseException);
    }
    SECTION("incomplete unicode escape") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"("\u12")"), JsonParseException);
    }
    SECTION("non-hex digits in unicode escape") {
        REQUIRE_THROWS_AS(JsonParser::Parse(R"("\uZZZZ")"), JsonParseException);
    }
    SECTION("leading zero followed by more digits") {
        REQUIRE_THROWS_AS(JsonParser::Parse("01"), JsonParseException);
    }
    SECTION("decimal point with no digits after it") {
        REQUIRE_THROWS_AS(JsonParser::Parse("1."), JsonParseException);
    }
    SECTION("bare minus, no digit") {
        REQUIRE_THROWS_AS(JsonParser::Parse("-"), JsonParseException);
    }
    SECTION("NaN is not a valid JSON literal") {
        REQUIRE_THROWS_AS(JsonParser::Parse("NaN"), JsonParseException);
    }
    SECTION("Infinity is not a valid JSON literal") {
        REQUIRE_THROWS_AS(JsonParser::Parse("Infinity"), JsonParseException);
    }
    SECTION("raw unescaped control character (literal tab) inside a string") {
        REQUIRE_THROWS_AS(JsonParser::Parse("\"a\tb\""), JsonParseException);
    }
}

TEST_CASE("JsonParseException carries an accurate line number", "[json][json-parser]") {
    // Missing value after the colon, on line 2.
    const std::string text = "{\n  \"a\": ,\n}";

    try {
        JsonParser::Parse(text);
        FAIL("expected JsonParseException to be thrown");
    } catch (const JsonParseException& ex) {
        REQUIRE(ex.Line() == 2);
        REQUIRE(ex.Column() > 0);
    }
}
