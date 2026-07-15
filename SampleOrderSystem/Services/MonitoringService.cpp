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

namespace {
// Mirrors OrderService::ComputeUnclaimedStock exactly: currentStock minus
// quantity already claimed by that sample's own PRODUCING/CONFIRMED orders,
// floored at 0. Duplicated here (rather than shared) because OrderService
// keeps that computation private to its own approval-time contract; this is
// a read-only monitoring projection of the same rule, not a second source of
// truth for it.
int ComputeUnclaimedStock(int currentStock, const std::string& sampleId, OrderRepository& orders) {
    int claimed = 0;
    for (const Order& order : orders.FindBySampleId(sampleId)) {
        if (order.status == OrderStatus::Producing || order.status == OrderStatus::Confirmed) {
            claimed += order.quantity;
        }
    }
    const int unclaimed = currentStock - claimed;
    return unclaimed > 0 ? unclaimed : 0;
}
}  // namespace

std::vector<SampleStockInfo> MonitoringService::GetSampleStockLevels() {
    production_.SettleDueEntries(clock_);

    std::vector<SampleStockInfo> result;
    for (const Sample& sample : samples_.FindAll()) {
        SampleStockInfo info;
        info.sampleId = sample.sampleId;
        info.sampleName = sample.name;
        info.currentStock = sample.currentStock;
        info.unclaimedStock = ComputeUnclaimedStock(sample.currentStock, sample.sampleId, orders_);
        info.level = sample.currentStock <= 0 ? StockLevel::Depleted : StockLevel::InStock;
        result.push_back(info);
    }
    return result;
}
