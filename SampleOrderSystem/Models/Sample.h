#pragma once
#include <string>
#include "../Json/JsonValue.h"


struct Sample {
    std::string sampleId;
    std::string name;
    double averageProductionTimeMinutes;  // positive real number of minutes; not required to be integral
    double yield;
    int currentStock;

    JsonValue ToJson() const;
    static Sample FromJson(const JsonValue& json);
};
