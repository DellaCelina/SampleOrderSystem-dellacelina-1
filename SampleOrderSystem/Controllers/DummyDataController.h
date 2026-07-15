#pragma once
#include <iostream>

#include "../Services/DummyDataGenerator.h"

class DummyDataController {
public:
    explicit DummyDataController(DummyDataGenerator& generator, std::ostream& out = std::cout);

    void Run(int sampleCount = 10, int orderCount = 20);

private:
    DummyDataGenerator& generator_;
    std::ostream& out_;
};
