#include "DataMonitorView.h"

#include "../Core/Console.h"

DataMonitorView::DataMonitorView(std::ostream& out) : out_(out) {}

void DataMonitorView::WriteHeader(const std::string& title) const {
    out_ << HeaderBlock() << title << "\n" << SeparatorLine() << "\n";
}

void DataMonitorView::Render(const std::vector<Sample>& samples, const std::vector<Order>& orders,
                              const std::vector<ProductionQueueEntry>& queueEntries) {
    WriteHeader("데이터 모니터");

    out_ << "[시료]\n";
    if (samples.empty()) {
        out_ << "등록된 시료 기록이 없습니다.\n";
    } else {
        for (const Sample& sample : samples) {
            out_ << sample.sampleId << " | " << sample.name << " | " << sample.currentStock << "\n";
        }
    }

    out_ << "\n[주문]\n";
    if (orders.empty()) {
        out_ << "등록된 주문 기록이 없습니다.\n";
    } else {
        for (const Order& order : orders) {
            out_ << order.orderNumber << " | " << order.sampleId << " | " << order.customerName << " | "
                 << order.quantity << " | " << ColorizeStatus(order.status, OrderStatusToString(order.status)) << "\n";
        }
    }

    out_ << "\n[생산 대기열]\n";
    if (queueEntries.empty()) {
        out_ << "등록된 생산 대기열 기록이 없습니다.\n";
    } else {
        for (const ProductionQueueEntry& entry : queueEntries) {
            out_ << entry.orderNumber << " | " << entry.sampleId << " | " << entry.shortfallQuantity << " | "
                 << entry.actualProducedQuantity << "\n";
        }
    }
}
