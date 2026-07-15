#include "ProductionLineViewService.h"

#include "../Models/ProductionQueueEntry.h"
#include "../Models/Sample.h"
#include "../Repositories/ProductionQueueRepository.h"
#include "../Repositories/SampleRepository.h"

namespace {

ProductionQueueEntryView ToView(const ProductionQueueEntry& entry, SampleRepository& samples) {
    ProductionQueueEntryView view;
    view.orderNumber = entry.orderNumber;
    view.sampleId = entry.sampleId;
    std::optional<Sample> sample = samples.FindById(entry.sampleId);
    view.sampleName = sample.has_value() ? sample->name : entry.sampleId;
    view.shortfallQuantity = entry.shortfallQuantity;
    view.actualProducedQuantity = entry.actualProducedQuantity;
    view.expectedCompletionAt = entry.expectedCompletionAt;
    return view;
}

}  // namespace

ProductionLineViewService::ProductionLineViewService(ProductionQueueRepository& queue,
                                                      OrderRepository& orders,
                                                      SampleRepository& samples,
                                                      IClock& clock)
    : queue_(queue), orders_(orders), samples_(samples), clock_(clock),
      production_(samples, orders, queue) {}

ProductionLineSnapshot ProductionLineViewService::GetSnapshot() {
    production_.SettleDueEntries(clock_);

    ProductionLineSnapshot snapshot;
    std::vector<ProductionQueueEntry> entries = queue_.FindAllInOrder();
    if (entries.empty()) {
        return snapshot;
    }

    snapshot.inProduction = ToView(entries.front(), samples_);
    for (size_t i = 1; i < entries.size(); ++i) {
        snapshot.waiting.push_back(ToView(entries[i], samples_));
    }
    return snapshot;
}
