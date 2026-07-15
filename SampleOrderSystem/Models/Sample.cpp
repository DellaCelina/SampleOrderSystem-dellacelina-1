#include "Sample.h"

#include <stdexcept>

namespace {

const JsonValue& RequireField(const JsonValue& json, const std::string& key) {
    if (!json.Has(key)) {
        throw std::invalid_argument("Sample::FromJson: missing required field \"" + key + "\"");
    }
    return json.Get(key);
}

}  // namespace

JsonValue Sample::ToJson() const {
    JsonValue json = JsonValue::MakeObject();
    json.Set("sampleId", sampleId);
    json.Set("name", name);
    json.Set("averageProductionTimeMinutes", averageProductionTimeMinutes);
    json.Set("yield", yield);
    json.Set("currentStock", currentStock);
    return json;
}

Sample Sample::FromJson(const JsonValue& json) {
    if (!json.IsObject()) {
        throw std::invalid_argument("Sample::FromJson: expected a JSON object");
    }

    Sample sample;

    const JsonValue& sampleId = RequireField(json, "sampleId");
    if (!sampleId.IsString()) {
        throw std::invalid_argument("Sample::FromJson: \"sampleId\" must be a string");
    }
    sample.sampleId = sampleId.AsString();

    const JsonValue& name = RequireField(json, "name");
    if (!name.IsString()) {
        throw std::invalid_argument("Sample::FromJson: \"name\" must be a string");
    }
    sample.name = name.AsString();

    const JsonValue& averageProductionTimeMinutes = RequireField(json, "averageProductionTimeMinutes");
    if (!averageProductionTimeMinutes.IsNumber()) {
        throw std::invalid_argument("Sample::FromJson: \"averageProductionTimeMinutes\" must be a number");
    }
    sample.averageProductionTimeMinutes = averageProductionTimeMinutes.AsNumber();

    const JsonValue& yield = RequireField(json, "yield");
    if (!yield.IsNumber()) {
        throw std::invalid_argument("Sample::FromJson: \"yield\" must be a number");
    }
    sample.yield = yield.AsNumber();

    const JsonValue& currentStock = RequireField(json, "currentStock");
    if (!currentStock.IsNumber()) {
        throw std::invalid_argument("Sample::FromJson: \"currentStock\" must be a number");
    }
    sample.currentStock = static_cast<int>(currentStock.AsNumber());

    return sample;
}
