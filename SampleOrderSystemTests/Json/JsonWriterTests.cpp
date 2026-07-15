#include <gtest/gtest.h>

#include "Json/JsonParser.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"

#include <string>

TEST(JsonWriterTest, WriteNullTrueFalse) {
    EXPECT_EQ(JsonWriter::Write(JsonValue()), "null");
    EXPECT_EQ(JsonWriter::Write(JsonValue(true)), "true");
    EXPECT_EQ(JsonWriter::Write(JsonValue(false)), "false");
}

TEST(JsonWriterTest, WriteNumbersWithNoGratuitousTrailingDotZero) {
    EXPECT_EQ(JsonWriter::Write(JsonValue(5)), "5");
    EXPECT_EQ(JsonWriter::Write(JsonValue(0.9)), "0.9");
    EXPECT_EQ(JsonWriter::Write(JsonValue(-3)), "-3");
}

TEST(JsonWriterTest, WriteASimpleString) {
    EXPECT_EQ(JsonWriter::Write(JsonValue(std::string("hi"))), "\"hi\"");
}

TEST(JsonWriterTest, StringEscapingQuoteBackslashNewline) {
    JsonValue value(std::string("a\"b\\c\n"));

    EXPECT_EQ(JsonWriter::Write(value), "\"a\\\"b\\\\c\\n\"");
}

TEST(JsonWriterTest, ControlCharacterNotCoveredByANamedEscapeUsesUZeroZeroXX) {
    JsonValue value(std::string("\x01"));

    EXPECT_EQ(JsonWriter::Write(value), "\"\\u0001\"");
}

TEST(JsonWriterTest, WriteEmptyArrayAndEmptyObject) {
    EXPECT_EQ(JsonWriter::Write(JsonValue::MakeArray()), "[]");
    EXPECT_EQ(JsonWriter::Write(JsonValue::MakeObject()), "{}");
}

TEST(JsonWriterTest, WriteASmallNestedFixtureWithExactPrettyPrintedOutput) {
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

    EXPECT_EQ(JsonWriter::Write(object), expected);
}

TEST(JsonWriterTest, ObjectKeyOrderInOutputExactlyMatchesInsertionOrder) {
    JsonValue object = JsonValue::MakeObject();
    object.Set("zebra", 1);
    object.Set("apple", 2);

    const std::string output = JsonWriter::Write(object);
    const size_t zebraPos = output.find("zebra");
    const size_t applePos = output.find("apple");

    EXPECT_NE(zebraPos, std::string::npos);
    EXPECT_NE(applePos, std::string::npos);
    EXPECT_LT(zebraPos, applePos);
}

TEST(JsonWriterTest, RoundTripViaStructuralEqualityForBuilderConstructedTrees) {
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
        EXPECT_EQ(roundTripped, value);
    }
}

TEST(JsonWriterTest, RoundTripViaTextIsIdempotentFromTheSecondParseOnward) {
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

        EXPECT_EQ(secondParse, firstParse);
    }
}
