#pragma once
#include <string>
#include <vector>

#include "../Core/IClock.h"
#include "../Models/Order.h"
#include "ProductionService.h"

class OrderRepository;
class SampleRepository;
class ProductionQueueRepository;

struct OrderStatusCounts {
    int reserved = 0;
    int confirmed = 0;
    int producing = 0;
    int released = 0;
    // Rejected is intentionally NOT a field here -- always excluded from monitoring.
};

enum class StockLevel { InStock, Depleted };

struct SampleStockInfo {
    std::string sampleId;
    std::string sampleName;
    int currentStock = 0;
    StockLevel level = StockLevel::InStock;  // Depleted iff currentStock == 0
};

// Read-only monitoring query service: settles due entries first, then reports
// order-status tallies and per-sample stock levels.
class MonitoringService {
public:
    MonitoringService(OrderRepository& orders,
                       SampleRepository& samples,
                       ProductionQueueRepository& queue,
                       IClock& clock);

    OrderStatusCounts GetOrderStatusCounts();

    std::vector<SampleStockInfo> GetSampleStockLevels();

private:
    OrderRepository& orders_;
    SampleRepository& samples_;
    ProductionQueueRepository& queue_;
    IClock& clock_;
    ProductionService production_;
};
