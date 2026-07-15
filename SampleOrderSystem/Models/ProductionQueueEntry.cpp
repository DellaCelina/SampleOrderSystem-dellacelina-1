#include "ProductionQueueEntry.h"

#include "../Core/Iso8601.h"

#include <stdexcept>

namespace {

const JsonValue& RequireField(const JsonValue& json, const std::string& key) {
    if (!json.Has(key)) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: missing required field \"" + key + "\"");
    }
    return json.Get(key);
}

}  // namespace

JsonValue ProductionQueueEntry::ToJson() const {
    JsonValue json = JsonValue::MakeObject();
    json.Set("orderNumber", orderNumber);
    json.Set("sampleId", sampleId);
    json.Set("shortfallQuantity", shortfallQuantity);
    json.Set("actualProducedQuantity", actualProducedQuantity);
    json.Set("enqueuedAt", TimePointToIso8601(enqueuedAt));
    json.Set("expectedCompletionAt", TimePointToIso8601(expectedCompletionAt));
    return json;
}

ProductionQueueEntry ProductionQueueEntry::FromJson(const JsonValue& json) {
    ProductionQueueEntry entry;

    const JsonValue& orderNumber = RequireField(json, "orderNumber");
    if (!orderNumber.IsString()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"orderNumber\" must be a string");
    }
    entry.orderNumber = orderNumber.AsString();

    const JsonValue& sampleId = RequireField(json, "sampleId");
    if (!sampleId.IsString()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"sampleId\" must be a string");
    }
    entry.sampleId = sampleId.AsString();

    const JsonValue& shortfallQuantity = RequireField(json, "shortfallQuantity");
    if (!shortfallQuantity.IsNumber()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"shortfallQuantity\" must be a number");
    }
    entry.shortfallQuantity = static_cast<int>(shortfallQuantity.AsNumber());

    const JsonValue& actualProducedQuantity = RequireField(json, "actualProducedQuantity");
    if (!actualProducedQuantity.IsNumber()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"actualProducedQuantity\" must be a number");
    }
    entry.actualProducedQuantity = static_cast<int>(actualProducedQuantity.AsNumber());

    const JsonValue& enqueuedAt = RequireField(json, "enqueuedAt");
    if (!enqueuedAt.IsString()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"enqueuedAt\" must be a string");
    }
    entry.enqueuedAt = ParseIso8601(enqueuedAt.AsString());

    const JsonValue& expectedCompletionAt = RequireField(json, "expectedCompletionAt");
    if (!expectedCompletionAt.IsString()) {
        throw std::invalid_argument("ProductionQueueEntry::FromJson: \"expectedCompletionAt\" must be a string");
    }
    entry.expectedCompletionAt = ParseIso8601(expectedCompletionAt.AsString());

    return entry;
}
