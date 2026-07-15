#include <gtest/gtest.h>

#include "Json/JsonValue.h"

#include <stdexcept>
#include <string>

TEST(JsonValueTest, DefaultConstructedJsonValueIsNull) {
    JsonValue value;

    EXPECT_EQ(value.Type(), JsonType::Null);
    EXPECT_TRUE(value.IsNull());
    EXPECT_FALSE(value.IsBool());
    EXPECT_FALSE(value.IsNumber());
    EXPECT_FALSE(value.IsString());
    EXPECT_FALSE(value.IsArray());
    EXPECT_FALSE(value.IsObject());
}

TEST(JsonValueTest, JsonValueNullptrIsAlsoNull) {
    JsonValue value(nullptr);

    EXPECT_EQ(value.Type(), JsonType::Null);
    EXPECT_TRUE(value.IsNull());
}

TEST(JsonValueTest, JsonValueBoolConstruction) {
    JsonValue trueValue(true);
    JsonValue falseValue(false);

    EXPECT_EQ(trueValue.Type(), JsonType::Bool);
    EXPECT_TRUE(trueValue.IsBool());
    EXPECT_EQ(trueValue.AsBool(), true);

    EXPECT_EQ(falseValue.Type(), JsonType::Bool);
    EXPECT_EQ(falseValue.AsBool(), false);
}

TEST(JsonValueTest, JsonValueNumberConstructionDoubleAndIntOverload) {
    JsonValue doubleValue(3.5);
    JsonValue intValue(5);

    EXPECT_EQ(doubleValue.Type(), JsonType::Number);
    EXPECT_EQ(doubleValue.AsNumber(), 3.5);

    EXPECT_EQ(intValue.Type(), JsonType::Number);
    EXPECT_EQ(intValue.AsNumber(), 5.0);
}

TEST(JsonValueTest, JsonValueStringConstructionStdStringAndConstCharPtr) {
    JsonValue stringValue(std::string("hello"));
    JsonValue charValue("hello");

    EXPECT_EQ(stringValue.Type(), JsonType::String);
    EXPECT_EQ(stringValue.AsString(), "hello");

    EXPECT_EQ(charValue.Type(), JsonType::String);
    EXPECT_EQ(charValue.AsString(), "hello");
}

TEST(JsonValueTest, MakeArrayThenPushHeterogeneousValues) {
    JsonValue array = JsonValue::MakeArray();
    JsonValue nested = JsonValue::MakeObject();
    nested.Set("k", "v");

    array.Push(JsonValue(1.0));
    array.Push(JsonValue("two"));
    array.Push(nested);

    EXPECT_EQ(array.Type(), JsonType::Array);
    EXPECT_EQ(array.AsArray().size(), 3);
    EXPECT_EQ(array.AsArray()[0].AsNumber(), 1.0);
    EXPECT_EQ(array.AsArray()[1].AsString(), "two");
    EXPECT_TRUE(array.AsArray()[2].IsObject());
    EXPECT_EQ(array.AsArray()[2].Get("k").AsString(), "v");
}

TEST(JsonValueTest, MakeArrayWithZeroPushCallsIsAnEmptyArrayNotNull) {
    JsonValue array = JsonValue::MakeArray();

    EXPECT_EQ(array.Type(), JsonType::Array);
    EXPECT_TRUE(array.AsArray().empty());
}

TEST(JsonValueTest, MakeObjectThenSetPreservesExactInsertionOrder) {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 1);
    object.Set("b", "two");
    object.Set("c", true);

    const auto& entries = object.AsObject();
    EXPECT_EQ(entries.size(), 3);
    EXPECT_EQ(entries[0].first, "a");
    EXPECT_EQ(entries[1].first, "b");
    EXPECT_EQ(entries[2].first, "c");
    EXPECT_EQ(entries[0].second.AsNumber(), 1.0);
    EXPECT_EQ(entries[1].second.AsString(), "two");
    EXPECT_EQ(entries[2].second.AsBool(), true);
}

TEST(JsonValueTest, SetOnAnExistingKeyReplacesValueInPlaceWithoutMovingPosition) {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 1);
    object.Set("b", "two");
    object.Set("a", 2);

    const auto& entries = object.AsObject();
    EXPECT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].first, "a");
    EXPECT_EQ(entries[0].second.AsNumber(), 2.0);
    EXPECT_EQ(entries[1].first, "b");
}

TEST(JsonValueTest, HasGetTryGetOnAPresentKey) {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 42);

    EXPECT_TRUE(object.Has("a"));
    EXPECT_EQ(object.Get("a").AsNumber(), 42.0);

    const JsonValue* found = object.TryGet("a");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->AsNumber(), 42.0);
}

TEST(JsonValueTest, HasTryGetGetOnAnAbsentKey) {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 42);

    EXPECT_FALSE(object.Has("missing"));
    EXPECT_EQ(object.TryGet("missing"), nullptr);
    EXPECT_THROW(object.Get("missing"), std::out_of_range);
}

TEST(JsonValueTest, HasTryGetGetOnANonObjectValue) {
    JsonValue number(5.0);

    EXPECT_FALSE(number.Has("a"));
    EXPECT_EQ(number.TryGet("a"), nullptr);
    EXPECT_THROW(number.Get("a"), std::out_of_range);
}

TEST(JsonValueTest, WrongTypeAccessorsThrowJsonTypeException_AsBoolOnNumber) {
    JsonValue value(5.0);
    EXPECT_THROW(value.AsBool(), JsonTypeException);
}

TEST(JsonValueTest, WrongTypeAccessorsThrowJsonTypeException_AsNumberOnString) {
    JsonValue value("hi");
    EXPECT_THROW(value.AsNumber(), JsonTypeException);
}

TEST(JsonValueTest, WrongTypeAccessorsThrowJsonTypeException_AsStringOnBool) {
    JsonValue value(true);
    EXPECT_THROW(value.AsString(), JsonTypeException);
}

TEST(JsonValueTest, WrongTypeAccessorsThrowJsonTypeException_AsArrayOnObject) {
    JsonValue value = JsonValue::MakeObject();
    EXPECT_THROW(value.AsArray(), JsonTypeException);
}

TEST(JsonValueTest, WrongTypeAccessorsThrowJsonTypeException_AsObjectOnArray) {
    JsonValue value = JsonValue::MakeArray();
    EXPECT_THROW(value.AsObject(), JsonTypeException);
}

TEST(JsonValueTest, PushOnANonArrayValueThrowsAndDoesNotConvert_DefaultConstructedNull) {
    JsonValue value;
    EXPECT_THROW(value.Push(JsonValue(1)), JsonTypeException);
    EXPECT_EQ(value.Type(), JsonType::Null);
}

TEST(JsonValueTest, PushOnANonArrayValueThrowsAndDoesNotConvert_Number) {
    JsonValue value(5.0);
    EXPECT_THROW(value.Push(JsonValue(1)), JsonTypeException);
    EXPECT_EQ(value.Type(), JsonType::Number);
}

TEST(JsonValueTest, SetOnANonObjectValueThrowsAndDoesNotConvert) {
    JsonValue value;
    EXPECT_THROW(value.Set("a", 1), JsonTypeException);
    EXPECT_EQ(value.Type(), JsonType::Null);
}

namespace {

JsonValue BuildJsonValueEqualityFixture() {
    JsonValue inner = JsonValue::MakeObject();
    inner.Set("x", 1);
    inner.Set("y", 2);

    JsonValue array = JsonValue::MakeArray();
    array.Push(inner);
    array.Push(JsonValue("leaf"));

    JsonValue outer = JsonValue::MakeObject();
    outer.Set("items", array);
    return outer;
}

}  // namespace

TEST(JsonValueTest, OperatorEqualsIsAStructuralOrderSensitiveDeepEquality) {
    JsonValue a = BuildJsonValueEqualityFixture();
    JsonValue b = BuildJsonValueEqualityFixture();
    EXPECT_EQ(a, b);
}

TEST(JsonValueTest, OperatorEqualsIsAStructuralOrderSensitiveDeepEquality_ChangingALeafValueMakesThemUnequal) {
    JsonValue a = BuildJsonValueEqualityFixture();
    JsonValue b = BuildJsonValueEqualityFixture();

    JsonValue& innerArray = b.AsObject()[0].second.AsArray()[0];
    innerArray.AsObject()[0].second = JsonValue(999);
    EXPECT_NE(a, b);
}

TEST(JsonValueTest, OperatorEqualsIsAStructuralOrderSensitiveDeepEquality_ChangingKeyOrderMakesThemUnequal) {
    JsonValue a = BuildJsonValueEqualityFixture();

    JsonValue reordered = JsonValue::MakeObject();
    JsonValue inner = JsonValue::MakeObject();
    inner.Set("y", 2);
    inner.Set("x", 1);
    JsonValue array = JsonValue::MakeArray();
    array.Push(inner);
    array.Push(JsonValue("leaf"));
    reordered.Set("items", array);

    EXPECT_NE(a, reordered);
}

TEST(JsonValueTest, CopyingAJsonValueIsIndependentOfTheOriginalValueSemantics) {
    JsonValue original = JsonValue::MakeArray();
    original.Push(JsonValue(1));

    JsonValue copy = original;
    copy.AsArray().push_back(JsonValue(2));

    EXPECT_EQ(original.AsArray().size(), 1);
    EXPECT_EQ(copy.AsArray().size(), 2);

    JsonValue originalObject = JsonValue::MakeObject();
    originalObject.Set("a", 1);

    JsonValue copyObject = originalObject;
    copyObject.Set("b", 2);

    EXPECT_EQ(originalObject.AsObject().size(), 1);
    EXPECT_EQ(copyObject.AsObject().size(), 2);
}
