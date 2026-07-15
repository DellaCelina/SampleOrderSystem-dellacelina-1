#pragma once
#include <string>
#include "../Json/JsonValue.h"


struct Sample {
    std::string sampleId;
    std::string name;
    int averageProductionTimeMinutes;
    double yield;
    int currentStock;

    JsonValue ToJson() const;
    static Sample FromJson(const JsonValue& json);
};
