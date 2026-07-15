#pragma once
#include <string>
#include "../Core/IClock.h"
#include "../Json/JsonValue.h"


struct ProductionQueueEntry {
    std::string orderNumber;
    std::string sampleId;
    int shortfallQuantity;
    int actualProducedQuantity;
    std::chrono::system_clock::time_point enqueuedAt;
    std::chrono::system_clock::time_point expectedCompletionAt;

    JsonValue ToJson() const;
    static ProductionQueueEntry FromJson(const JsonValue& json);
};
