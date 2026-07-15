#include "DataMonitorView.h"

DataMonitorView::DataMonitorView(std::ostream& out) : out_(out) {}

void DataMonitorView::Render(const std::vector<Sample>& samples, const std::vector<Order>& orders,
                              const std::vector<ProductionQueueEntry>& queueEntries) {
    out_ << "Samples\n";
    if (samples.empty()) {
        out_ << "No sample records found.\n";
    } else {
        for (const Sample& sample : samples) {
            out_ << sample.sampleId << " | " << sample.name << " | " << sample.currentStock << "\n";
        }
    }

    out_ << "Orders\n";
    if (orders.empty()) {
        out_ << "No order records found.\n";
    } else {
        for (const Order& order : orders) {
            out_ << order.orderNumber << " | " << order.sampleId << " | " << order.customerName << " | "
                 << order.quantity << " | " << OrderStatusToString(order.status) << "\n";
        }
    }

    out_ << "Production Queue\n";
    if (queueEntries.empty()) {
        out_ << "No production queue records found.\n";
    } else {
        for (const ProductionQueueEntry& entry : queueEntries) {
            out_ << entry.orderNumber << " | " << entry.sampleId << " | " << entry.shortfallQuantity << " | "
                 << entry.actualProducedQuantity << "\n";
        }
    }
}
