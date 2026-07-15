#include "DataMonitorController.h"

DataMonitorController::DataMonitorController(IClock& clock, ProductionService& productionService,
                                               SampleRepository& sampleRepository, OrderRepository& orderRepository,
                                               ProductionQueueRepository& productionQueueRepository,
                                               DataMonitorView& view)
    : clock_(clock),
      productionService_(productionService),
      sampleRepository_(sampleRepository),
      orderRepository_(orderRepository),
      productionQueueRepository_(productionQueueRepository),
      view_(view) {}

void DataMonitorController::Run() {
    productionService_.SettleDueEntries(clock_);

    view_.Render(sampleRepository_.FindAll(), orderRepository_.FindAll(), productionQueueRepository_.FindAllInOrder());
}
