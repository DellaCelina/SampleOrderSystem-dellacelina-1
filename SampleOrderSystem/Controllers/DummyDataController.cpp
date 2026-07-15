#include "DummyDataController.h"

DummyDataController::DummyDataController(DummyDataGenerator& generator, std::ostream& out)
    : generator_(generator), out_(out) {}

void DummyDataController::Run(int sampleCount, int orderCount) {
    DummyDataOptions options;
    options.sampleCount = sampleCount;
    options.orderCount = orderCount;
    generator_.GenerateAll(options);

    out_ << "시료 " << sampleCount << "개, 주문 " << orderCount << "개를 생성했습니다.\n";
}
