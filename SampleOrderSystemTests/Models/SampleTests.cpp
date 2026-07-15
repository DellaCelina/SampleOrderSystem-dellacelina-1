#include <gtest/gtest.h>

#include "Models/Sample.h"
#include "Json/JsonValue.h"

#include <stdexcept>

namespace {

JsonValue MakeValidSampleJson() {
    JsonValue json = JsonValue::MakeObject();
    json.Set("sampleId", "SMP-001");
    json.Set("name", "Test Sample");
    json.Set("averageProductionTimeMinutes", 45);
    json.Set("yield", 0.9);
    json.Set("currentStock", 120);
    return json;
}

}  // namespace

TEST(SampleTest, SampleRoundTripsThroughToJsonFromJson) {
    Sample sample;
    sample.sampleId = "SMP-042";
    sample.name = "Widget Sample";
    sample.averageProductionTimeMinutes = 30;
    sample.yield = 0.85;
    sample.currentStock = 250;

    const JsonValue json = sample.ToJson();
    const Sample roundTripped = Sample::FromJson(json);

    EXPECT_EQ(roundTripped.sampleId, sample.sampleId);
    EXPECT_EQ(roundTripped.name, sample.name);
    EXPECT_EQ(roundTripped.averageProductionTimeMinutes, sample.averageProductionTimeMinutes);
    EXPECT_EQ(roundTripped.yield, sample.yield);
    EXPECT_EQ(roundTripped.currentStock, sample.currentStock);
}

TEST(SampleTest, SampleFromJsonThrowsWhenARequiredFieldIsMissing_MissingSampleId) {
    JsonValue json = MakeValidSampleJson();
    JsonValue withoutField = JsonValue::MakeObject();
    withoutField.Set("name", json.Get("name"));
    withoutField.Set("averageProductionTimeMinutes", json.Get("averageProductionTimeMinutes"));
    withoutField.Set("yield", json.Get("yield"));
    withoutField.Set("currentStock", json.Get("currentStock"));
    EXPECT_THROW(Sample::FromJson(withoutField), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenARequiredFieldIsMissing_MissingName) {
    JsonValue json = MakeValidSampleJson();
    JsonValue withoutField = JsonValue::MakeObject();
    withoutField.Set("sampleId", json.Get("sampleId"));
    withoutField.Set("averageProductionTimeMinutes", json.Get("averageProductionTimeMinutes"));
    withoutField.Set("yield", json.Get("yield"));
    withoutField.Set("currentStock", json.Get("currentStock"));
    EXPECT_THROW(Sample::FromJson(withoutField), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenARequiredFieldIsMissing_MissingAverageProductionTimeMinutes) {
    JsonValue json = MakeValidSampleJson();
    JsonValue withoutField = JsonValue::MakeObject();
    withoutField.Set("sampleId", json.Get("sampleId"));
    withoutField.Set("name", json.Get("name"));
    withoutField.Set("yield", json.Get("yield"));
    withoutField.Set("currentStock", json.Get("currentStock"));
    EXPECT_THROW(Sample::FromJson(withoutField), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenARequiredFieldIsMissing_MissingYield) {
    JsonValue json = MakeValidSampleJson();
    JsonValue withoutField = JsonValue::MakeObject();
    withoutField.Set("sampleId", json.Get("sampleId"));
    withoutField.Set("name", json.Get("name"));
    withoutField.Set("averageProductionTimeMinutes", json.Get("averageProductionTimeMinutes"));
    withoutField.Set("currentStock", json.Get("currentStock"));
    EXPECT_THROW(Sample::FromJson(withoutField), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenARequiredFieldIsMissing_MissingCurrentStock) {
    JsonValue json = MakeValidSampleJson();
    JsonValue withoutField = JsonValue::MakeObject();
    withoutField.Set("sampleId", json.Get("sampleId"));
    withoutField.Set("name", json.Get("name"));
    withoutField.Set("averageProductionTimeMinutes", json.Get("averageProductionTimeMinutes"));
    withoutField.Set("yield", json.Get("yield"));
    EXPECT_THROW(Sample::FromJson(withoutField), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenAFieldHasTheWrongJsonType) {
    JsonValue json = MakeValidSampleJson();
    json.Set("currentStock", "not-a-number");

    EXPECT_THROW(Sample::FromJson(json), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonThrowsWhenTopLevelValueIsNotAnObject) {
    EXPECT_THROW(Sample::FromJson(JsonValue::MakeArray()), std::invalid_argument);
}

TEST(SampleTest, SampleFromJsonDoesNotThrowOnOutOfDomainButWellTypedValues) {
    JsonValue json = MakeValidSampleJson();
    json.Set("yield", 2.0);  // out of the [0,1] business range, but a well-typed Number

    Sample sample;
    EXPECT_NO_THROW(sample = Sample::FromJson(json));
    EXPECT_EQ(sample.yield, 2.0);
}
