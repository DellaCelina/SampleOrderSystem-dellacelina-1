#pragma once
#include "../Core/IClock.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/ProductionQueueRepository.h"
#include "../Repositories/SampleRepository.h"
#include "../Services/ProductionService.h"
#include "../Views/DataMonitorView.h"

class DataMonitorController {
public:
    DataMonitorController(IClock& clock, ProductionService& productionService, SampleRepository& sampleRepository,
                           OrderRepository& orderRepository, ProductionQueueRepository& productionQueueRepository,
                           DataMonitorView& view);

    void Run();

private:
    IClock& clock_;
    ProductionService& productionService_;
    SampleRepository& sampleRepository_;
    OrderRepository& orderRepository_;
    ProductionQueueRepository& productionQueueRepository_;
    DataMonitorView& view_;
};
