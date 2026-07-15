#include "DummyDataController.h"

DummyDataController::DummyDataController(DummyDataGenerator& generator, std::ostream& out)
    : generator_(generator), out_(out) {}

void DummyDataController::Run(int sampleCount, int orderCount) {
    DummyDataOptions options;
    options.sampleCount = sampleCount;
    options.orderCount = orderCount;
    generator_.GenerateAll(options);

    out_ << "Generated " << sampleCount << " sample(s) and " << orderCount << " order(s).\n";
}
