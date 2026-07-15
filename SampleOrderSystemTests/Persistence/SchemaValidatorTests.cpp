#include <gtest/gtest.h>

#include "Persistence/Schema.h"
#include "Persistence/SchemaValidator.h"
#include "Json/JsonValue.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "Models/ProductionQueueEntry.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

FieldSchema MakeStringField(const std::string& name, bool required = true) {
    FieldSchema field;
    field.name = name;
    field.type = FieldType::String;
    field.required = required;
    return field;
}

FieldSchema MakeIntegerField(const std::string& name, std::optional<double> min = std::nullopt,
                              std::optional<double> max = std::nullopt) {
    FieldSchema field;
    field.name = name;
    field.type = FieldType::Integer;
    field.required = true;
    field.min = min;
    field.max = max;
    return field;
}

// Schema modeled directly on order.schema.json (orderNumber pattern, status
// enum, quantity min 1) -- exercises pattern/enum/bounds together.
Schema MakeOrderLikeSchema() {
    Schema schema;
    schema.tableName = "orders";

    FieldSchema orderNumber = MakeStringField("orderNumber");
    orderNumber.pattern = "^ORD-\\d{4}$";

    FieldSchema sampleId = MakeStringField("sampleId");

    FieldSchema quantity = MakeIntegerField("quantity", 1.0, std::nullopt);

    FieldSchema status = MakeStringField("status");
    status.enumValues = std::vector<std::string>{"RESERVED", "CONFIRMED", "PRODUCING", "RELEASED", "REJECTED"};

    schema.fields = {orderNumber, sampleId, quantity, status};
    return schema;
}

// Schema modeled on sample.schema.json's yield field: exclusiveMin 0, max 1.
Schema MakeSampleLikeSchema() {
    Schema schema;
    schema.tableName = "samples";

    FieldSchema sampleId = MakeStringField("sampleId");

    FieldSchema averageProductionTimeMinutes = MakeIntegerField("averageProductionTimeMinutes", 1.0, std::nullopt);

    FieldSchema yield;
    yield.name = "yield";
    yield.type = FieldType::Number;
    yield.required = true;
    yield.exclusiveMin = 0.0;
    yield.max = 1.0;

    FieldSchema currentStock = MakeIntegerField("currentStock", 0.0, std::nullopt);

    schema.fields = {sampleId, averageProductionTimeMinutes, yield, currentStock};
    return schema;
}

// Schema modeled on production_queue.schema.json's enqueuedAt field.
Schema MakeIso8601FieldSchema() {
    Schema schema;
    schema.tableName = "production_queue";

    FieldSchema enqueuedAt;
    enqueuedAt.name = "enqueuedAt";
    enqueuedAt.type = FieldType::String;
    enqueuedAt.required = true;
    enqueuedAt.isIso8601Format = true;

    schema.fields = {enqueuedAt};
    return schema;
}

JsonValue MakeValidOrderRecord() {
    JsonValue record = JsonValue::MakeObject();
    record.Set("orderNumber", "ORD-0001");
    record.Set("sampleId", "SMP-001");
    record.Set("quantity", 5);
    record.Set("status", "RESERVED");
    return record;
}

}  // namespace

TEST(SchemaValidatorTest, ValidatesAWellFormedArrayOfRecordsSuccessfully) {
    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidOrderRecord());

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, TopLevelValueThatIsNotAnArrayIsAnError) {
    JsonValue data = JsonValue::MakeObject();
    data.Set("orderNumber", "ORD-0001");

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(error->recordIndex, std::nullopt);
}

TEST(SchemaValidatorTest, AnArrayElementThatIsNotAnObjectIsAnErrorNamingItsIndex) {
    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidOrderRecord());
    data.Push(JsonValue("not-an-object"));

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    ASSERT_TRUE(error->recordIndex.has_value());
    EXPECT_EQ(*error->recordIndex, 1u);
}

TEST(SchemaValidatorTest, MissingRequiredFieldIsAnErrorNamingRecordIndexAndFieldName) {
    JsonValue record = MakeValidOrderRecord();
    JsonValue withoutStatus = JsonValue::MakeObject();
    withoutStatus.Set("orderNumber", record.Get("orderNumber"));
    withoutStatus.Set("sampleId", record.Get("sampleId"));
    withoutStatus.Set("quantity", record.Get("quantity"));

    JsonValue data = JsonValue::MakeArray();
    data.Push(withoutStatus);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    ASSERT_TRUE(error->recordIndex.has_value());
    EXPECT_EQ(*error->recordIndex, 0u);
    ASSERT_TRUE(error->fieldName.has_value());
    EXPECT_EQ(*error->fieldName, "status");
}

TEST(SchemaValidatorTest, MissingOptionalFieldIsNotAnError) {
    Schema schema = MakeOrderLikeSchema();
    schema.fields.push_back(MakeStringField("notes", /*required=*/false));

    JsonValue data = JsonValue::MakeArray();
    data.Push(MakeValidOrderRecord());  // "notes" omitted entirely

    std::optional<ValidationError> error = Validate(data, schema);

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, StringFieldGivenANumberIsATypeError) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("orderNumber", 1234);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "orderNumber");
}

TEST(SchemaValidatorTest, IntegerFieldGivenAStringIsATypeError) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("quantity", "5");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "quantity");
}

TEST(SchemaValidatorTest, StringFieldGivenABooleanIsATypeError) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("sampleId", true);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "sampleId");
}

TEST(SchemaValidatorTest, IntegerFieldGivenANonIntegralNumberIsAnError) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("quantity", 3.5);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "quantity");
}

TEST(SchemaValidatorTest, IntegerFieldExactlyAtMinBoundaryPasses) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("quantity", 1);  // min is 1

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, IntegerFieldOneBelowMinBoundaryFails) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("quantity", 0);  // min is 1

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "quantity");
}

TEST(SchemaValidatorTest, NumericFieldExactlyAtInclusiveMaxBoundaryPasses) {
    Schema schema = MakeSampleLikeSchema();
    JsonValue record = JsonValue::MakeObject();
    record.Set("sampleId", "SMP-001");
    record.Set("averageProductionTimeMinutes", 10);
    record.Set("yield", 1.0);  // max is 1, inclusive
    record.Set("currentStock", 0);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, schema);

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, NumericFieldOneAboveInclusiveMaxBoundaryFails) {
    Schema schema = MakeSampleLikeSchema();
    JsonValue record = JsonValue::MakeObject();
    record.Set("sampleId", "SMP-001");
    record.Set("averageProductionTimeMinutes", 10);
    record.Set("yield", 1.5);  // above max of 1
    record.Set("currentStock", 0);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, schema);

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "yield");
}

TEST(SchemaValidatorTest, NumericFieldExactlyAtExclusiveMinBoundaryFails) {
    Schema schema = MakeSampleLikeSchema();
    JsonValue record = JsonValue::MakeObject();
    record.Set("sampleId", "SMP-001");
    record.Set("averageProductionTimeMinutes", 10);
    record.Set("yield", 0.0);  // exclusiveMin is 0, so 0 itself must fail
    record.Set("currentStock", 0);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, schema);

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "yield");
}

TEST(SchemaValidatorTest, NumericFieldJustAboveExclusiveMinAndWithinMaxPasses) {
    Schema schema = MakeSampleLikeSchema();
    JsonValue record = JsonValue::MakeObject();
    record.Set("sampleId", "SMP-001");
    record.Set("averageProductionTimeMinutes", 10);
    record.Set("yield", 0.9);
    record.Set("currentStock", 0);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, schema);

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, PatternFieldMatchingOrdFourDigitsPasses) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("orderNumber", "ORD-0001");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, PatternFieldWithWrongDigitCountFails) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("orderNumber", "ORD-1");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "orderNumber");
}

TEST(SchemaValidatorTest, PatternFieldWithWrongCaseFails) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("orderNumber", "ord-0001");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "orderNumber");
}

TEST(SchemaValidatorTest, PatternFieldWithTooManyDigitsFails) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("orderNumber", "ORD-00001");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "orderNumber");
}

TEST(SchemaValidatorTest, EnumFieldWithAnAllowedValuePasses) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("status", "CONFIRMED");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, EnumFieldWithADisallowedValueFails) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("status", "Cancelled");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "status");
}

TEST(SchemaValidatorTest, Iso8601FormatCurrentlyAcceptsCalendarImpossibleDayOfMonthByDesign) {
    // Documents a deliberate limitation: the ISO-8601 format check only
    // validates structural shape + coarse range sanity (month 01-12, day
    // 01-31), not full calendar correctness (leap years, day-of-month
    // exactness per month). "2026-02-30" (February has no 30th) currently
    // passes. If this is ever tightened, this test should be updated
    // deliberately rather than the behavior silently changing.
    FieldSchema field;
    field.name = "enqueuedAt";
    field.type = FieldType::String;
    field.required = true;
    field.isIso8601Format = true;

    Schema schema;
    schema.tableName = "production_queue";
    schema.fields = {field};

    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", "2026-02-30T00:00:00Z");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    EXPECT_EQ(Validate(data, schema), std::nullopt);
}

namespace {

// Resolves the checked-in schema/*.schema.json path relative to this test
// file's own location (stable regardless of the test binary's working
// directory, which is not yet decided -- see phase-14's open item), rather
// than a hardcoded relative path that could break depending on how the test
// executable is launched.
std::string RealSchemaPath(const std::string& fileName) {
    std::filesystem::path here = std::filesystem::path(__FILE__).parent_path();
    return (here / ".." / ".." / "schema" / fileName).string();
}

}  // namespace

TEST(SchemaValidatorTest, RealOrderSchemaFileAcceptsARealOrderToJsonOutput) {
    Schema schema = LoadSchemaFromFile(RealSchemaPath("order.schema.json"));

    Order order;
    order.orderNumber = "ORD-0001";
    order.sampleId = "SMP-001";
    order.customerName = "Acme Corp";
    order.quantity = 10;
    order.status = OrderStatus::Reserved;

    JsonValue data = JsonValue::MakeArray();
    data.Push(order.ToJson());

    EXPECT_EQ(Validate(data, schema), std::nullopt);
}

TEST(SchemaValidatorTest, RealSampleSchemaFileAcceptsARealSampleToJsonOutput) {
    Schema schema = LoadSchemaFromFile(RealSchemaPath("sample.schema.json"));

    Sample sample;
    sample.sampleId = "SMP-001";
    sample.name = "Widget";
    sample.averageProductionTimeMinutes = 30;
    sample.yield = 0.9;
    sample.currentStock = 100;

    JsonValue data = JsonValue::MakeArray();
    data.Push(sample.ToJson());

    EXPECT_EQ(Validate(data, schema), std::nullopt);
}

TEST(SchemaValidatorTest, RealProductionQueueSchemaFileAcceptsARealProductionQueueEntryToJsonOutput) {
    Schema schema = LoadSchemaFromFile(RealSchemaPath("production_queue.schema.json"));

    ProductionQueueEntry entry;
    entry.orderNumber = "ORD-0001";
    entry.sampleId = "SMP-001";
    entry.shortfallQuantity = 10;
    entry.actualProducedQuantity = 12;
    entry.enqueuedAt = std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}};
    entry.expectedCompletionAt = std::chrono::system_clock::time_point{std::chrono::seconds{1704070800}};

    JsonValue data = JsonValue::MakeArray();
    data.Push(entry.ToJson());

    EXPECT_EQ(Validate(data, schema), std::nullopt);
}

TEST(SchemaValidatorTest, Iso8601FormatFieldWithAWellFormedTimestampPasses) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", "2026-07-15T10:30:00Z");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeIso8601FieldSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, Iso8601FormatFieldMissingTAndZFails) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", "2026-07-15 10:30:00");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeIso8601FieldSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "enqueuedAt");
}

TEST(SchemaValidatorTest, Iso8601FormatFieldMissingTrailingZFails) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", "2026-07-15T10:30:00");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeIso8601FieldSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "enqueuedAt");
}

TEST(SchemaValidatorTest, Iso8601FormatFieldWithOutOfRangeMonthFails) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", "2026-13-01T00:00:00Z");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeIso8601FieldSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "enqueuedAt");
}

TEST(SchemaValidatorTest, Iso8601FormatFieldGivenANonStringValueIsATypeErrorNotAFormatError) {
    JsonValue record = JsonValue::MakeObject();
    record.Set("enqueuedAt", 12345);

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeIso8601FieldSchema());

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(*error->fieldName, "enqueuedAt");
}

TEST(SchemaValidatorTest, ExtraUnknownFieldInARecordIsTolerated) {
    JsonValue record = MakeValidOrderRecord();
    record.Set("unexpectedExtraField", "surprise");

    JsonValue data = JsonValue::MakeArray();
    data.Push(record);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    EXPECT_EQ(error, std::nullopt);
}

TEST(SchemaValidatorTest, FailsFastOnTheFirstBadRecordNotConfusedByAnEarlierGoodOne) {
    JsonValue goodRecord = MakeValidOrderRecord();
    JsonValue badRecord = MakeValidOrderRecord();
    badRecord.Set("status", "NotARealStatus");

    JsonValue data = JsonValue::MakeArray();
    data.Push(goodRecord);
    data.Push(badRecord);

    std::optional<ValidationError> error = Validate(data, MakeOrderLikeSchema());

    ASSERT_TRUE(error.has_value());
    ASSERT_TRUE(error->recordIndex.has_value());
    EXPECT_EQ(*error->recordIndex, 1u);
}
