#include "MonitoringService.h"

#include "../Models/Sample.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/SampleRepository.h"

MonitoringService::MonitoringService(OrderRepository& orders,
                                     SampleRepository& samples,
                                     ProductionQueueRepository& queue,
                                     IClock& clock)
    : orders_(orders), samples_(samples), queue_(queue), clock_(clock),
      production_(samples, orders, queue) {}

OrderStatusCounts MonitoringService::GetOrderStatusCounts() {
    production_.SettleDueEntries(clock_);

    OrderStatusCounts counts;
    for (const Order& order : orders_.FindAll()) {
        switch (order.status) {
            case OrderStatus::Reserved:
                counts.reserved++;
                break;
            case OrderStatus::Confirmed:
                counts.confirmed++;
                break;
            case OrderStatus::Producing:
                counts.producing++;
                break;
            case OrderStatus::Released:
                counts.released++;
                break;
            case OrderStatus::Rejected:
                break;
        }
    }
    return counts;
}

std::vector<SampleStockInfo> MonitoringService::GetSampleStockLevels() {
    production_.SettleDueEntries(clock_);

    std::vector<SampleStockInfo> result;
    for (const Sample& sample : samples_.FindAll()) {
        SampleStockInfo info;
        info.sampleId = sample.sampleId;
        info.sampleName = sample.name;
        info.currentStock = sample.currentStock;
        info.level = sample.currentStock <= 0 ? StockLevel::Depleted : StockLevel::InStock;
        result.push_back(info);
    }
    return result;
}
