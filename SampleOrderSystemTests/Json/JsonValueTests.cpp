#include "catch_amalgamated.hpp"

#include "Json/JsonValue.h"

#include <stdexcept>
#include <string>

TEST_CASE("Default-constructed JsonValue is Null", "[json][json-value]") {
    JsonValue value;

    REQUIRE(value.Type() == JsonType::Null);
    REQUIRE(value.IsNull());
    REQUIRE_FALSE(value.IsBool());
    REQUIRE_FALSE(value.IsNumber());
    REQUIRE_FALSE(value.IsString());
    REQUIRE_FALSE(value.IsArray());
    REQUIRE_FALSE(value.IsObject());
}

TEST_CASE("JsonValue(nullptr) is also Null", "[json][json-value]") {
    JsonValue value(nullptr);

    REQUIRE(value.Type() == JsonType::Null);
    REQUIRE(value.IsNull());
}

TEST_CASE("JsonValue bool construction", "[json][json-value]") {
    JsonValue trueValue(true);
    JsonValue falseValue(false);

    REQUIRE(trueValue.Type() == JsonType::Bool);
    REQUIRE(trueValue.IsBool());
    REQUIRE(trueValue.AsBool() == true);

    REQUIRE(falseValue.Type() == JsonType::Bool);
    REQUIRE(falseValue.AsBool() == false);
}

TEST_CASE("JsonValue number construction (double and int overload)", "[json][json-value]") {
    JsonValue doubleValue(3.5);
    JsonValue intValue(5);

    REQUIRE(doubleValue.Type() == JsonType::Number);
    REQUIRE(doubleValue.AsNumber() == 3.5);

    REQUIRE(intValue.Type() == JsonType::Number);
    REQUIRE(intValue.AsNumber() == 5.0);
}

TEST_CASE("JsonValue string construction (std::string and const char*)", "[json][json-value]") {
    JsonValue stringValue(std::string("hello"));
    JsonValue charValue("hello");

    REQUIRE(stringValue.Type() == JsonType::String);
    REQUIRE(stringValue.AsString() == "hello");

    REQUIRE(charValue.Type() == JsonType::String);
    REQUIRE(charValue.AsString() == "hello");
}

TEST_CASE("MakeArray then Push heterogeneous values", "[json][json-value]") {
    JsonValue array = JsonValue::MakeArray();
    JsonValue nested = JsonValue::MakeObject();
    nested.Set("k", "v");

    array.Push(JsonValue(1.0));
    array.Push(JsonValue("two"));
    array.Push(nested);

    REQUIRE(array.Type() == JsonType::Array);
    REQUIRE(array.AsArray().size() == 3);
    REQUIRE(array.AsArray()[0].AsNumber() == 1.0);
    REQUIRE(array.AsArray()[1].AsString() == "two");
    REQUIRE(array.AsArray()[2].IsObject());
    REQUIRE(array.AsArray()[2].Get("k").AsString() == "v");
}

TEST_CASE("MakeArray with zero Push calls is an empty Array, not Null", "[json][json-value]") {
    JsonValue array = JsonValue::MakeArray();

    REQUIRE(array.Type() == JsonType::Array);
    REQUIRE(array.AsArray().empty());
}

TEST_CASE("MakeObject then Set preserves exact insertion order", "[json][json-value]") {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 1);
    object.Set("b", "two");
    object.Set("c", true);

    const auto& entries = object.AsObject();
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].first == "a");
    REQUIRE(entries[1].first == "b");
    REQUIRE(entries[2].first == "c");
    REQUIRE(entries[0].second.AsNumber() == 1.0);
    REQUIRE(entries[1].second.AsString() == "two");
    REQUIRE(entries[2].second.AsBool() == true);
}

TEST_CASE("Set on an existing key replaces value in place without moving position", "[json][json-value]") {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 1);
    object.Set("b", "two");
    object.Set("a", 2);

    const auto& entries = object.AsObject();
    REQUIRE(entries.size() == 2);
    REQUIRE(entries[0].first == "a");
    REQUIRE(entries[0].second.AsNumber() == 2.0);
    REQUIRE(entries[1].first == "b");
}

TEST_CASE("Has/Get/TryGet on a present key", "[json][json-value]") {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 42);

    REQUIRE(object.Has("a"));
    REQUIRE(object.Get("a").AsNumber() == 42.0);

    const JsonValue* found = object.TryGet("a");
    REQUIRE(found != nullptr);
    REQUIRE(found->AsNumber() == 42.0);
}

TEST_CASE("Has/TryGet/Get on an absent key", "[json][json-value]") {
    JsonValue object = JsonValue::MakeObject();
    object.Set("a", 42);

    REQUIRE_FALSE(object.Has("missing"));
    REQUIRE(object.TryGet("missing") == nullptr);
    REQUIRE_THROWS_AS(object.Get("missing"), std::out_of_range);
}

TEST_CASE("Has/TryGet/Get on a non-Object value", "[json][json-value]") {
    JsonValue number(5.0);

    REQUIRE_FALSE(number.Has("a"));
    REQUIRE(number.TryGet("a") == nullptr);
    REQUIRE_THROWS_AS(number.Get("a"), std::out_of_range);
}

TEST_CASE("Wrong-type accessors throw JsonTypeException", "[json][json-value]") {
    SECTION("AsBool on Number") {
        JsonValue value(5.0);
        REQUIRE_THROWS_AS(value.AsBool(), JsonTypeException);
    }
    SECTION("AsNumber on String") {
        JsonValue value("hi");
        REQUIRE_THROWS_AS(value.AsNumber(), JsonTypeException);
    }
    SECTION("AsString on Bool") {
        JsonValue value(true);
        REQUIRE_THROWS_AS(value.AsString(), JsonTypeException);
    }
    SECTION("AsArray on Object") {
        JsonValue value = JsonValue::MakeObject();
        REQUIRE_THROWS_AS(value.AsArray(), JsonTypeException);
    }
    SECTION("AsObject on Array") {
        JsonValue value = JsonValue::MakeArray();
        REQUIRE_THROWS_AS(value.AsObject(), JsonTypeException);
    }
}

TEST_CASE("Push on a non-Array value throws and does not convert", "[json][json-value]") {
    SECTION("default-constructed Null") {
        JsonValue value;
        REQUIRE_THROWS_AS(value.Push(JsonValue(1)), JsonTypeException);
        REQUIRE(value.Type() == JsonType::Null);
    }
    SECTION("Number") {
        JsonValue value(5.0);
        REQUIRE_THROWS_AS(value.Push(JsonValue(1)), JsonTypeException);
        REQUIRE(value.Type() == JsonType::Number);
    }
}

TEST_CASE("Set on a non-Object value throws and does not convert", "[json][json-value]") {
    JsonValue value;
    REQUIRE_THROWS_AS(value.Set("a", 1), JsonTypeException);
    REQUIRE(value.Type() == JsonType::Null);
}

TEST_CASE("operator== is a structural, order-sensitive deep equality", "[json][json-value]") {
    auto build = []() {
        JsonValue inner = JsonValue::MakeObject();
        inner.Set("x", 1);
        inner.Set("y", 2);

        JsonValue array = JsonValue::MakeArray();
        array.Push(inner);
        array.Push(JsonValue("leaf"));

        JsonValue outer = JsonValue::MakeObject();
        outer.Set("items", array);
        return outer;
    };

    JsonValue a = build();
    JsonValue b = build();
    REQUIRE(a == b);

    SECTION("changing a leaf value makes them unequal") {
        JsonValue& innerArray = b.AsObject()[0].second.AsArray()[0];
        innerArray.AsObject()[0].second = JsonValue(999);
        REQUIRE(a != b);
    }

    SECTION("changing key order makes them unequal") {
        JsonValue reordered = JsonValue::MakeObject();
        JsonValue inner = JsonValue::MakeObject();
        inner.Set("y", 2);
        inner.Set("x", 1);
        JsonValue array = JsonValue::MakeArray();
        array.Push(inner);
        array.Push(JsonValue("leaf"));
        reordered.Set("items", array);

        REQUIRE(a != reordered);
    }
}

TEST_CASE("Copying a JsonValue is independent of the original (value semantics)", "[json][json-value]") {
    JsonValue original = JsonValue::MakeArray();
    original.Push(JsonValue(1));

    JsonValue copy = original;
    copy.AsArray().push_back(JsonValue(2));

    REQUIRE(original.AsArray().size() == 1);
    REQUIRE(copy.AsArray().size() == 2);

    JsonValue originalObject = JsonValue::MakeObject();
    originalObject.Set("a", 1);

    JsonValue copyObject = originalObject;
    copyObject.Set("b", 2);

    REQUIRE(originalObject.AsObject().size() == 1);
    REQUIRE(copyObject.AsObject().size() == 2);
}
