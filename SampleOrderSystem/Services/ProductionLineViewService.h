#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "../Core/IClock.h"
#include "ProductionService.h"

class ProductionQueueRepository;
class OrderRepository;
class SampleRepository;

struct ProductionQueueEntryView {
    std::string orderNumber;
    std::string sampleId;
    std::string sampleName;
    int shortfallQuantity = 0;
    int actualProducedQuantity = 0;
    std::chrono::system_clock::time_point expectedCompletionAt;
};

struct ProductionLineSnapshot {
    // The FIFO head, i.e. the entry currently "in production" -- empty when the queue is empty.
    std::optional<ProductionQueueEntryView> inProduction;
    // The remaining FIFO tail, in queue order, NOT including the head above.
    std::vector<ProductionQueueEntryView> waiting;
};

// Read-only production-line query service: settles due entries first, then
// reports the FIFO queue head as "in production" plus the remaining tail.
class ProductionLineViewService {
public:
    ProductionLineViewService(ProductionQueueRepository& queue,
                               OrderRepository& orders,
                               SampleRepository& samples,
                               IClock& clock);

    ProductionLineSnapshot GetSnapshot();

private:
    ProductionQueueRepository& queue_;
    OrderRepository& orders_;
    SampleRepository& samples_;
    IClock& clock_;
    ProductionService production_;
};
