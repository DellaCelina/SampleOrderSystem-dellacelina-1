#include <gtest/gtest.h>

#include "Persistence/Schema.h"
#include "Json/JsonValue.h"
#include "Json/JsonParser.h"
#include "Json/JsonWriter.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

JsonValue MakeField(const std::string& name, const std::string& type, bool required = true) {
    JsonValue field = JsonValue::MakeObject();
    field.Set("name", name);
    field.Set("type", type);
    field.Set("required", required);
    return field;
}

JsonValue MakeSchemaDoc(const std::string& table, std::vector<JsonValue> fields) {
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("table", table);
    JsonValue fieldsArray = JsonValue::MakeArray();
    for (auto& f : fields) {
        fieldsArray.Push(f);
    }
    doc.Set("fields", fieldsArray);
    return doc;
}

// Mirrors schema/sample.schema.json's declared shape (architecture doc's
// Sample model): sampleId, name, averageProductionTimeMinutes, yield,
// currentStock.
JsonValue MakeSampleSchemaDoc() {
    JsonValue sampleId = MakeField("sampleId", "string");
    JsonValue name = MakeField("name", "string");
    JsonValue avgTime = MakeField("averageProductionTimeMinutes", "integer");
    avgTime.Set("min", 1);
    JsonValue yield = MakeField("yield", "number");
    yield.Set("exclusiveMin", 0);
    yield.Set("max", 1);
    JsonValue currentStock = MakeField("currentStock", "integer");
    currentStock.Set("min", 0);
    return MakeSchemaDoc("samples", {sampleId, name, avgTime, yield, currentStock});
}

// Mirrors schema/order.schema.json.
JsonValue MakeOrderSchemaDoc() {
    JsonValue orderNumber = MakeField("orderNumber", "string");
    orderNumber.Set("pattern", "^ORD-\\d{4}$");
    JsonValue sampleId = MakeField("sampleId", "string");
    JsonValue customerName = MakeField("customerName", "string");
    JsonValue quantity = MakeField("quantity", "integer");
    quantity.Set("min", 1);
    JsonValue status = MakeField("status", "string");
    JsonValue enumValues = JsonValue::MakeArray();
    enumValues.Push(JsonValue("RESERVED"));
    enumValues.Push(JsonValue("CONFIRMED"));
    enumValues.Push(JsonValue("PRODUCING"));
    enumValues.Push(JsonValue("RELEASED"));
    enumValues.Push(JsonValue("REJECTED"));
    status.Set("enum", enumValues);
    return MakeSchemaDoc("orders", {orderNumber, sampleId, customerName, quantity, status});
}

// Mirrors schema/production_queue.schema.json.
JsonValue MakeProductionQueueSchemaDoc() {
    JsonValue orderNumber = MakeField("orderNumber", "string");
    orderNumber.Set("pattern", "^ORD-\\d{4}$");
    JsonValue sampleId = MakeField("sampleId", "string");
    JsonValue shortfallQuantity = MakeField("shortfallQuantity", "integer");
    shortfallQuantity.Set("min", 1);
    JsonValue actualProducedQuantity = MakeField("actualProducedQuantity", "integer");
    actualProducedQuantity.Set("min", 1);
    JsonValue enqueuedAt = MakeField("enqueuedAt", "string");
    enqueuedAt.Set("format", "iso8601");
    JsonValue expectedCompletionAt = MakeField("expectedCompletionAt", "string");
    expectedCompletionAt.Set("format", "iso8601");
    return MakeSchemaDoc("production_queue",
                          {orderNumber, sampleId, shortfallQuantity, actualProducedQuantity, enqueuedAt,
                           expectedCompletionAt});
}

}  // namespace

TEST(SchemaTest, ParsesTheSampleSchemaDocumentAndSpotChecksFields) {
    Schema schema = Schema::FromJson(MakeSampleSchemaDoc());

    EXPECT_EQ(schema.tableName, "samples");
    ASSERT_EQ(schema.fields.size(), 5u);

    const FieldSchema& yieldField = schema.fields[3];
    EXPECT_EQ(yieldField.name, "yield");
    EXPECT_EQ(yieldField.type, FieldType::Number);
    ASSERT_TRUE(yieldField.exclusiveMin.has_value());
    EXPECT_DOUBLE_EQ(*yieldField.exclusiveMin, 0.0);
    ASSERT_TRUE(yieldField.max.has_value());
    EXPECT_DOUBLE_EQ(*yieldField.max, 1.0);

    const FieldSchema& stockField = schema.fields[4];
    EXPECT_EQ(stockField.name, "currentStock");
    EXPECT_EQ(stockField.type, FieldType::Integer);
    ASSERT_TRUE(stockField.min.has_value());
    EXPECT_DOUBLE_EQ(*stockField.min, 0.0);
}

TEST(SchemaTest, ParsesTheOrderSchemaDocumentAndSpotChecksPatternAndEnum) {
    Schema schema = Schema::FromJson(MakeOrderSchemaDoc());

    EXPECT_EQ(schema.tableName, "orders");
    ASSERT_EQ(schema.fields.size(), 5u);

    const FieldSchema& orderNumberField = schema.fields[0];
    EXPECT_EQ(orderNumberField.name, "orderNumber");
    ASSERT_TRUE(orderNumberField.pattern.has_value());
    EXPECT_EQ(*orderNumberField.pattern, "^ORD-\\d{4}$");

    const FieldSchema& statusField = schema.fields[4];
    EXPECT_EQ(statusField.name, "status");
    ASSERT_TRUE(statusField.enumValues.has_value());
    ASSERT_EQ(statusField.enumValues->size(), 5u);
    EXPECT_EQ((*statusField.enumValues)[0], "RESERVED");
    EXPECT_EQ((*statusField.enumValues)[4], "REJECTED");
}

TEST(SchemaTest, ParsesTheProductionQueueSchemaDocumentAndSpotChecksIso8601Format) {
    Schema schema = Schema::FromJson(MakeProductionQueueSchemaDoc());

    EXPECT_EQ(schema.tableName, "production_queue");
    ASSERT_EQ(schema.fields.size(), 6u);

    const FieldSchema& enqueuedAtField = schema.fields[4];
    EXPECT_EQ(enqueuedAtField.name, "enqueuedAt");
    EXPECT_EQ(enqueuedAtField.type, FieldType::String);
    EXPECT_TRUE(enqueuedAtField.isIso8601Format);

    const FieldSchema& shortfallField = schema.fields[2];
    EXPECT_EQ(shortfallField.name, "shortfallQuantity");
    EXPECT_EQ(shortfallField.type, FieldType::Integer);
    ASSERT_TRUE(shortfallField.min.has_value());
    EXPECT_DOUBLE_EQ(*shortfallField.min, 1.0);
}

TEST(SchemaTest, FromJsonThrowsWhenTableKeyIsMissing) {
    JsonValue doc = JsonValue::MakeObject();
    JsonValue fields = JsonValue::MakeArray();
    fields.Push(MakeField("a", "string"));
    doc.Set("fields", fields);

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsWhenFieldsKeyIsMissing) {
    JsonValue doc = JsonValue::MakeObject();
    doc.Set("table", "things");

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsWhenAFieldIsMissingName) {
    JsonValue field = JsonValue::MakeObject();
    field.Set("type", "string");
    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsWhenAFieldIsMissingType) {
    JsonValue field = JsonValue::MakeObject();
    field.Set("name", "a");
    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsOnUnrecognizedTypeValue) {
    JsonValue field = MakeField("a", "array");  // "array" is not in {string,integer,number,boolean}
    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsOnUnrecognizedFormatValue) {
    JsonValue field = MakeField("a", "string");
    field.Set("format", "date");  // only "iso8601" is recognized

    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsWhenPatternIsSetOnANonStringField) {
    JsonValue field = MakeField("a", "integer");
    field.Set("pattern", "^\\d+$");

    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, FromJsonThrowsWhenEnumIsSetOnANonStringField) {
    JsonValue field = MakeField("a", "number");
    JsonValue enumValues = JsonValue::MakeArray();
    enumValues.Push(JsonValue("x"));
    field.Set("enum", enumValues);

    JsonValue doc = MakeSchemaDoc("things", {field});

    EXPECT_THROW(Schema::FromJson(doc), std::runtime_error);
}

TEST(SchemaTest, RequiredDefaultsToTrueWhenOmitted) {
    JsonValue field = JsonValue::MakeObject();
    field.Set("name", "a");
    field.Set("type", "string");
    // "required" intentionally omitted.

    JsonValue doc = MakeSchemaDoc("things", {field});
    Schema schema = Schema::FromJson(doc);

    ASSERT_EQ(schema.fields.size(), 1u);
    EXPECT_TRUE(schema.fields[0].required);
}

TEST(SchemaTest, LoadSchemaFromFileReadsParsesAndBuildsASchemaFromDisk) {
    const std::string tempPath = ::testing::TempDir() + "SchemaTest_LoadSchemaFromFile.schema.json";
    {
        std::ofstream out(tempPath, std::ios::trunc);
        out << JsonWriter::Write(MakeSampleSchemaDoc());
    }

    Schema schema = LoadSchemaFromFile(tempPath);

    EXPECT_EQ(schema.tableName, "samples");
    ASSERT_EQ(schema.fields.size(), 5u);
    EXPECT_EQ(schema.fields[0].name, "sampleId");

    std::remove(tempPath.c_str());
}

TEST(SchemaTest, LoadSchemaFromFileThrowsWhenTheFileDoesNotExist) {
    const std::string missingPath = ::testing::TempDir() + "SchemaTest_DoesNotExist_" + "nope.schema.json";

    EXPECT_THROW(LoadSchemaFromFile(missingPath), std::exception);
}
