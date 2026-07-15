#include <gtest/gtest.h>

#include "Json/JsonParser.h"
#include "Json/JsonValue.h"

#include <string>

TEST(JsonParserTest, ParseLiteralsTrueFalseNull) {
    JsonValue trueValue = JsonParser::Parse("true");
    EXPECT_EQ(trueValue.Type(), JsonType::Bool);
    EXPECT_EQ(trueValue.AsBool(), true);

    JsonValue falseValue = JsonParser::Parse("false");
    EXPECT_EQ(falseValue.Type(), JsonType::Bool);
    EXPECT_EQ(falseValue.AsBool(), false);

    JsonValue nullValue = JsonParser::Parse("null");
    EXPECT_TRUE(nullValue.IsNull());
}

TEST(JsonParserTest, ParseNumbersInVariousForms) {
    EXPECT_EQ(JsonParser::Parse("42").AsNumber(), 42.0);
    EXPECT_EQ(JsonParser::Parse("-7").AsNumber(), -7.0);
    EXPECT_EQ(JsonParser::Parse("0").AsNumber(), 0.0);
    EXPECT_EQ(JsonParser::Parse("3.14").AsNumber(), 3.14);
    EXPECT_EQ(JsonParser::Parse("-0.5").AsNumber(), -0.5);
    EXPECT_EQ(JsonParser::Parse("1e10").AsNumber(), 1e10);
    EXPECT_EQ(JsonParser::Parse("1.5E-3").AsNumber(), 1.5E-3);
    EXPECT_EQ(JsonParser::Parse("2e+5").AsNumber(), 2e+5);
}

TEST(JsonParserTest, ParseAStringWithEveryNamedEscape) {
    JsonValue value = JsonParser::Parse("\"\\\"a\\\\b/c\\bd\\fe\\nf\\rg\\th\\\"\"");

    std::string expected = "\"a\\b/c\bd\fe\nf\rg\th\"";
    EXPECT_EQ(value.AsString(), expected);
}

TEST(JsonParserTest, ParseAUEscapeForABmpCharacter) {
    JsonValue value = JsonParser::Parse("\"\\u00e9\"");

    EXPECT_EQ(value.AsString(), "\xC3\xA9"); // UTF-8 encoding of U+00E9 (é)
}

TEST(JsonParserTest, ParseEmptyArrayAndEmptyObject) {
    JsonValue array = JsonParser::Parse("[]");
    EXPECT_EQ(array.Type(), JsonType::Array);
    EXPECT_TRUE(array.AsArray().empty());

    JsonValue object = JsonParser::Parse("{}");
    EXPECT_EQ(object.Type(), JsonType::Object);
    EXPECT_TRUE(object.AsObject().empty());
}

TEST(JsonParserTest, ParseANestedStructureWithHeterogeneousLeafTypes) {
    const std::string text = R"([
        {"name": "a", "count": 1, "active": true, "note": null, "tags": ["x", "y"]},
        {"name": "b", "count": 2, "active": false, "note": null, "tags": []}
    ])";

    JsonValue root = JsonParser::Parse(text);
    EXPECT_EQ(root.Type(), JsonType::Array);
    EXPECT_EQ(root.AsArray().size(), 2);

    const JsonValue& first = root.AsArray()[0];
    EXPECT_EQ(first.Get("name").AsString(), "a");
    EXPECT_EQ(first.Get("count").AsNumber(), 1.0);
    EXPECT_EQ(first.Get("active").AsBool(), true);
    EXPECT_TRUE(first.Get("note").IsNull());
    EXPECT_EQ(first.Get("tags").AsArray().size(), 2);
    EXPECT_EQ(first.Get("tags").AsArray()[0].AsString(), "x");
    EXPECT_EQ(first.Get("tags").AsArray()[1].AsString(), "y");

    const JsonValue& second = root.AsArray()[1];
    EXPECT_EQ(second.Get("name").AsString(), "b");
    EXPECT_EQ(second.Get("active").AsBool(), false);
    EXPECT_TRUE(second.Get("tags").AsArray().empty());
}

TEST(JsonParserTest, LeadingTrailingWhitespaceAroundATopLevelValueIsTolerated) {
    JsonValue withWhitespace = JsonParser::Parse("  \n\t{ \"a\" : 1 }\n  ");
    JsonValue withoutWhitespace = JsonParser::Parse("{\"a\":1}");

    EXPECT_EQ(withWhitespace, withoutWhitespace);
}

TEST(JsonParserTest, DuplicateKeysInOneObjectLastValueWinsFirstPositionKept) {
    JsonValue value = JsonParser::Parse(R"({"a":1,"b":2,"a":3})");

    const auto& entries = value.AsObject();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].first, "a");
    EXPECT_EQ(entries[0].second.AsNumber(), 3.0);
    EXPECT_EQ(entries[1].first, "b");
    EXPECT_EQ(entries[1].second.AsNumber(), 2.0);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_EmptyString) {
    EXPECT_THROW(JsonParser::Parse(""), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_UnterminatedObject) {
    EXPECT_THROW(JsonParser::Parse("{"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_UnterminatedArray) {
    EXPECT_THROW(JsonParser::Parse("["), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_TrailingGarbageAfterACompleteValue) {
    EXPECT_THROW(JsonParser::Parse("123 abc"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_MissingColon) {
    EXPECT_THROW(JsonParser::Parse(R"({"a" 1})"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_MissingCommaBetweenArrayElements) {
    EXPECT_THROW(JsonParser::Parse("[1 2]"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_MissingCommaBetweenObjectMembers) {
    EXPECT_THROW(JsonParser::Parse(R"({"a":1 "b":2})"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_UnquotedKey) {
    EXPECT_THROW(JsonParser::Parse("{a:1}"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_SingleQuotedKeyString) {
    EXPECT_THROW(JsonParser::Parse("{'a':1}"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_TrailingCommaInArray) {
    EXPECT_THROW(JsonParser::Parse("[1,2,]"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_TrailingCommaInObject) {
    EXPECT_THROW(JsonParser::Parse(R"({"a":1,})"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_InvalidEscapeSequence) {
    EXPECT_THROW(JsonParser::Parse(R"("\q")"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_IncompleteUnicodeEscape) {
    EXPECT_THROW(JsonParser::Parse(R"("\u12")"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_NonHexDigitsInUnicodeEscape) {
    EXPECT_THROW(JsonParser::Parse(R"("\uZZZZ")"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_LeadingZeroFollowedByMoreDigits) {
    EXPECT_THROW(JsonParser::Parse("01"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_DecimalPointWithNoDigitsAfterIt) {
    EXPECT_THROW(JsonParser::Parse("1."), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_BareMinusNoDigit) {
    EXPECT_THROW(JsonParser::Parse("-"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_NaNIsNotAValidJsonLiteral) {
    EXPECT_THROW(JsonParser::Parse("NaN"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_InfinityIsNotAValidJsonLiteral) {
    EXPECT_THROW(JsonParser::Parse("Infinity"), JsonParseException);
}

TEST(JsonParserTest, MalformedInputThrowsJsonParseException_RawUnescapedControlCharacterLiteralTabInsideAString) {
    EXPECT_THROW(JsonParser::Parse("\"a\tb\""), JsonParseException);
}

TEST(JsonParserTest, JsonParseExceptionCarriesAnAccurateLineNumber) {
    // Missing value after the colon, on line 2.
    const std::string text = "{\n  \"a\": ,\n}";

    try {
        JsonParser::Parse(text);
        FAIL() << "expected JsonParseException to be thrown";
    } catch (const JsonParseException& ex) {
        EXPECT_EQ(ex.Line(), 2);
        EXPECT_GT(ex.Column(), 0);
    }
}
