#include "Services/ProductionService.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

ProductionService::ProductionService(SampleRepository& samples,
                                     OrderRepository& orders,
                                     ProductionQueueRepository& queue)
    : m_samples(samples), m_orders(orders), m_queue(queue) {}

int ProductionService::ComputeShortfall(int requestedQuantity, int unclaimedStock) {
    int shortfall = requestedQuantity - unclaimedStock;
    return shortfall > 0 ? shortfall : 0;
}

int ProductionService::ComputeActualQuantity(int shortfall, double yield) {
    if (shortfall <= 0) {
        return 0;
    }
    return static_cast<int>(std::ceil(static_cast<double>(shortfall) / yield));
}

int ProductionService::ComputeProductionDurationMinutes(int actualProducedQuantity,
                                                          int averageProductionTimeMinutes) {
    return actualProducedQuantity * averageProductionTimeMinutes;
}

std::chrono::system_clock::time_point ProductionService::ComputeCompletionTime(
    std::chrono::system_clock::time_point enqueuedAt,
    std::optional<std::chrono::system_clock::time_point> previousTailCompletion,
    int durationMinutes) {
    std::chrono::system_clock::time_point anchor =
        previousTailCompletion.has_value() ? std::max(enqueuedAt, *previousTailCompletion) : enqueuedAt;
    return anchor + std::chrono::minutes(durationMinutes);
}

ProductionQueueEntry ProductionService::Enqueue(const std::string& orderNumber,
                                                 const std::string& sampleId,
                                                 int shortfallQuantity,
                                                 IClock& clock) {
    std::optional<Sample> sample = m_samples.FindById(sampleId);
    if (!sample.has_value()) {
        throw std::invalid_argument("ProductionService::Enqueue: unknown sampleId \"" + sampleId + "\"");
    }

    int actualProducedQuantity = ComputeActualQuantity(shortfallQuantity, sample->yield);
    int durationMinutes = ComputeProductionDurationMinutes(actualProducedQuantity, sample->averageProductionTimeMinutes);

    std::chrono::system_clock::time_point enqueuedAt = clock.Now();

    std::vector<ProductionQueueEntry> existing = m_queue.FindAllInOrder();
    std::optional<std::chrono::system_clock::time_point> previousTailCompletion;
    if (!existing.empty()) {
        previousTailCompletion = existing.back().expectedCompletionAt;
    }

    std::chrono::system_clock::time_point completion =
        ComputeCompletionTime(enqueuedAt, previousTailCompletion, durationMinutes);

    ProductionQueueEntry entry;
    entry.orderNumber = orderNumber;
    entry.sampleId = sampleId;
    entry.shortfallQuantity = shortfallQuantity;
    entry.actualProducedQuantity = actualProducedQuantity;
    entry.enqueuedAt = enqueuedAt;
    entry.expectedCompletionAt = completion;

    m_queue.Enqueue(entry);

    return entry;
}

void ProductionService::SettleDueEntries(IClock& clock) {
    std::vector<ProductionQueueEntry> entries = m_queue.FindAllInOrder();
    std::chrono::system_clock::time_point now = clock.Now();

    for (const ProductionQueueEntry& entry : entries) {
        if (entry.expectedCompletionAt > now) {
            continue;
        }

        m_samples.IncreaseStock(entry.sampleId, entry.actualProducedQuantity);

        std::optional<Order> order = m_orders.FindByOrderNumber(entry.orderNumber);
        if (order.has_value() && order->status == OrderStatus::Producing) {
            m_orders.UpdateStatus(entry.orderNumber, OrderStatus::Confirmed);
        }
        // Defensive: if the order is missing or not currently Producing, this
        // should not happen given the invariant that only Producing orders
        // have a live queue entry -- skip the status mutation but still
        // remove the entry from the queue and still credit stock below, and
        // do not throw, so settlement stays total and non-blocking.

        m_queue.Remove(entry.orderNumber);
    }
}
