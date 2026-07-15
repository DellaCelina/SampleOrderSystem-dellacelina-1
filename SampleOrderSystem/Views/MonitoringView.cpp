#include "MonitoringView.h"

MonitoringView::MonitoringView(std::ostream& out) : out_(out) {}

void MonitoringView::RenderStatusCounts(const OrderStatusCounts& counts) {
    out_ << "Order Status Counts\n"
         << "RESERVED: " << counts.reserved << "\n"
         << "CONFIRMED: " << counts.confirmed << "\n"
         << "PRODUCING: " << counts.producing << "\n"
         << "RELEASED: " << counts.released << "\n";
}

void MonitoringView::RenderSampleStocks(const std::vector<SampleStockInfo>& stocks) {
    if (stocks.empty()) {
        out_ << "No sample stock records found.\n";
        return;
    }

    out_ << "Sample Stock Levels\n";
    for (const SampleStockInfo& stock : stocks) {
        out_ << stock.sampleId << " | " << stock.sampleName << " | " << stock.currentStock << " | "
             << (stock.level == StockLevel::Depleted ? "Depleted" : "InStock") << "\n";
    }
}
