#include "MonitoringView.h"

#include "../Core/Console.h"

MonitoringView::MonitoringView(std::ostream& out) : out_(out) {}

void MonitoringView::WriteHeader(const std::string& title) const {
    out_ << HeaderBlock() << title << "\n" << SeparatorLine() << "\n";
}

void MonitoringView::RenderStatusCounts(const OrderStatusCounts& counts) {
    WriteHeader("모니터링 요약");
    out_ << "[상태별 주문 현황]\n"
         << ColorizeStatus(OrderStatus::Reserved, OrderStatusToString(OrderStatus::Reserved)) << " : " << counts.reserved
         << "건\n"
         << ColorizeStatus(OrderStatus::Confirmed, OrderStatusToString(OrderStatus::Confirmed)) << " : "
         << counts.confirmed << "건\n"
         << ColorizeStatus(OrderStatus::Producing, OrderStatusToString(OrderStatus::Producing)) << " : "
         << counts.producing << "건\n"
         << ColorizeStatus(OrderStatus::Released, OrderStatusToString(OrderStatus::Released)) << " : " << counts.released
         << "건\n";
}

void MonitoringView::RenderSampleStocks(const std::vector<SampleStockInfo>& stocks) {
    out_ << "\n[재고 현황]\n";
    if (stocks.empty()) {
        out_ << "등록된 재고 정보가 없습니다.\n";
        return;
    }

    out_ << "ID | 시료명 | 재고 | 여유분(선점되지 않은 재고) | 상태\n";
    for (const SampleStockInfo& stock : stocks) {
        const bool depleted = stock.level == StockLevel::Depleted;
        const std::string label = depleted ? Colorize(AnsiColor::kRed, "고갈") : Colorize(AnsiColor::kGreen, "여유");
        out_ << stock.sampleId << " | " << stock.sampleName << " | " << stock.currentStock << " | "
             << stock.unclaimedStock << " | " << label << "\n";
    }
}
