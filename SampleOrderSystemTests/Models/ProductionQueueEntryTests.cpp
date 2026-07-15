#include <gtest/gtest.h>

#include "Models/ProductionQueueEntry.h"
#include "Json/JsonValue.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::chrono::system_clock::time_point MakeUtcSeconds(long long seconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}

JsonValue MakeValidEntryJson() {
    JsonValue json = JsonValue::MakeObject();
    json.Set("orderNumber", "ORD-0001");
    json.Set("sampleId", "SMP-001");
    json.Set("shortfallQuantity", 15);
    json.Set("actualProducedQuantity", 20);
    json.Set("enqueuedAt", "2026-07-15T10:00:00Z");
    json.Set("expectedCompletionAt", "2026-07-15T12:00:00Z");
    return json;
}

}  // namespace

TEST(ProductionQueueEntryTest, ProductionQueueEntryRoundTripsThroughToJsonFromJsonIncludingTimePoints) {
    ProductionQueueEntry entry;
    entry.orderNumber = "ORD-0099";
    entry.sampleId = "SMP-099";
    entry.shortfallQuantity = 30;
    entry.actualProducedQuantity = 34;
    entry.enqueuedAt = MakeUtcSeconds(1704067200);           // 2024-01-01T00:00:00Z
    entry.expectedCompletionAt = MakeUtcSeconds(1704070800); // 2024-01-01T01:00:00Z

    const JsonValue json = entry.ToJson();
    const ProductionQueueEntry roundTripped = ProductionQueueEntry::FromJson(json);

    EXPECT_EQ(roundTripped.orderNumber, entry.orderNumber);
    EXPECT_EQ(roundTripped.sampleId, entry.sampleId);
    EXPECT_EQ(roundTripped.shortfallQuantity, entry.shortfallQuantity);
    EXPECT_EQ(roundTripped.actualProducedQuantity, entry.actualProducedQuantity);
    EXPECT_EQ(roundTripped.enqueuedAt, entry.enqueuedAt);
    EXPECT_EQ(roundTripped.expectedCompletionAt, entry.expectedCompletionAt);
}

TEST(ProductionQueueEntryTest, ProductionQueueEntryFromJsonPropagatesParseIso8601sExceptionForMalformedTimestamps_EnqueuedAtIsMalformedMissingZ) {
    JsonValue json = MakeValidEntryJson();
    json.Set("enqueuedAt", "2026-07-15T10:00:00");
    EXPECT_THROW(ProductionQueueEntry::FromJson(json), std::invalid_argument);
}

TEST(ProductionQueueEntryTest, ProductionQueueEntryFromJsonPropagatesParseIso8601sExceptionForMalformedTimestamps_ExpectedCompletionAtIsMalformedMissingZ) {
    JsonValue json = MakeValidEntryJson();
    json.Set("expectedCompletionAt", "2026-07-15T12:00:00");
    EXPECT_THROW(ProductionQueueEntry::FromJson(json), std::invalid_argument);
}

TEST(ProductionQueueEntryTest, ProductionQueueEntryFromJsonThrowsWhenARequiredNonTimestampFieldIsMissing) {
    const std::vector<std::string> requiredFields = {
        "orderNumber", "sampleId", "shortfallQuantity", "actualProducedQuantity"};

    for (const auto& missingField : requiredFields) {
        JsonValue full = MakeValidEntryJson();
        JsonValue withoutField = JsonValue::MakeObject();
        withoutField.Set("enqueuedAt", full.Get("enqueuedAt"));
        withoutField.Set("expectedCompletionAt", full.Get("expectedCompletionAt"));
        for (const auto& field : requiredFields) {
            if (field != missingField) {
                withoutField.Set(field, full.Get(field));
            }
        }
        EXPECT_THROW(ProductionQueueEntry::FromJson(withoutField), std::invalid_argument);
    }
}

TEST(ProductionQueueEntryTest, ProductionQueueEntryFromJsonThrowsWhenANonTimestampFieldHasTheWrongJsonType) {
    JsonValue json = MakeValidEntryJson();
    json.Set("shortfallQuantity", "not-a-number");

    EXPECT_THROW(ProductionQueueEntry::FromJson(json), std::invalid_argument);
}

TEST(ProductionQueueEntryTest, TwoEntriesSharingTheSameUnderlyingTimePointSerializeToIdenticalIso8601Strings) {
    // Simulates ProductionService's later FIFO chain: entry A's completion
    // time seeds entry B's enqueue time. This phase only guarantees they
    // serialize identically through the single Iso8601 code path.
    const auto sharedTimePoint = MakeUtcSeconds(1704067200);

    ProductionQueueEntry entryA;
    entryA.orderNumber = "ORD-0001";
    entryA.sampleId = "SMP-001";
    entryA.shortfallQuantity = 10;
    entryA.actualProducedQuantity = 10;
    entryA.enqueuedAt = MakeUtcSeconds(1704060000);
    entryA.expectedCompletionAt = sharedTimePoint;

    ProductionQueueEntry entryB;
    entryB.orderNumber = "ORD-0002";
    entryB.sampleId = "SMP-001";
    entryB.shortfallQuantity = 5;
    entryB.actualProducedQuantity = 5;
    entryB.enqueuedAt = sharedTimePoint;
    entryB.expectedCompletionAt = MakeUtcSeconds(1704074400);

    const JsonValue jsonA = entryA.ToJson();
    const JsonValue jsonB = entryB.ToJson();

    EXPECT_EQ(jsonA.Get("expectedCompletionAt").AsString(), jsonB.Get("enqueuedAt").AsString());
}
