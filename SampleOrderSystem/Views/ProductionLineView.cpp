#include "ProductionLineView.h"

#include "../Core/Console.h"
#include "../Core/Iso8601.h"

ProductionLineView::ProductionLineView(std::ostream& out) : out_(out) {}

void ProductionLineView::WriteHeader(const std::string& title) const {
    out_ << HeaderBlock() << title << "\n" << SeparatorLine() << "\n";
}

void ProductionLineView::RenderEntry(const ProductionQueueEntryView& entry) {
    out_ << entry.orderNumber << " | " << entry.sampleId << " | " << entry.sampleName << " | "
         << entry.shortfallQuantity << " | " << entry.actualProducedQuantity << " | "
         << TimePointToIso8601(entry.expectedCompletionAt) << "\n";
}

void ProductionLineView::RenderSnapshot(const ProductionLineSnapshot& snapshot) {
    WriteHeader("생산 라인 조회");

    if (!snapshot.inProduction.has_value() && snapshot.waiting.empty()) {
        out_ << "생산 라인이 유휴 상태입니다. 대기 중인 항목이 없습니다.\n";
        return;
    }

    if (snapshot.inProduction.has_value()) {
        out_ << "[생산 중]\n";
        out_ << "주문번호 | 시료 | 부족분 | 실생산량 | 예상 완료\n";
        RenderEntry(*snapshot.inProduction);
        out_ << "\n";
    }

    if (!snapshot.waiting.empty()) {
        out_ << "[대기 중인 주문 (FIFO 순)]\n";
        out_ << "순서 | 주문번호 | 시료 | 부족분 | 실생산량 | 예상 완료\n";
        int order = 1;
        for (const ProductionQueueEntryView& entry : snapshot.waiting) {
            out_ << order << " | ";
            RenderEntry(entry);
            ++order;
        }
    }
}
