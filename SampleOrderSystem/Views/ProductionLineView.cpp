#include "ProductionLineView.h"

ProductionLineView::ProductionLineView(std::ostream& out) : out_(out) {}

void ProductionLineView::RenderEntry(const ProductionQueueEntryView& entry) {
    out_ << entry.orderNumber << " | " << entry.sampleId << " | " << entry.sampleName << " | "
         << entry.shortfallQuantity << " | " << entry.actualProducedQuantity << "\n";
}

void ProductionLineView::RenderSnapshot(const ProductionLineSnapshot& snapshot) {
    if (!snapshot.inProduction.has_value() && snapshot.waiting.empty()) {
        out_ << "Production line is idle. No entries in the queue.\n";
        return;
    }

    if (snapshot.inProduction.has_value()) {
        out_ << "In Production:\n";
        RenderEntry(*snapshot.inProduction);
    }

    if (!snapshot.waiting.empty()) {
        out_ << "Waiting:\n";
        for (const ProductionQueueEntryView& entry : snapshot.waiting) {
            RenderEntry(entry);
        }
    }
}
