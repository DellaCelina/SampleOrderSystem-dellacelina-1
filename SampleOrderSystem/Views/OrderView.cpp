#include "OrderView.h"

#include "../Core/Console.h"

OrderView::OrderView(std::ostream& out) : out_(out) {}

void OrderView::WriteHeader(const std::string& title) const {
    out_ << HeaderBlock() << title << "\n" << SeparatorLine() << "\n";
}

void OrderView::ShowOrderMenu() {
    WriteHeader("주문 관리");
    out_ << "1) 주문 접수\n"
         << "2) 승인 대기 목록 조회 (승인/거절)\n"
         << "3) 출고 처리\n"
         << "4) 뒤로가기\n"
         << "선택 > ";
}

void OrderView::ShowInvalidMenuChoice() {
    out_ << "오류: 올바르지 않은 메뉴 선택입니다.\n";
}

void OrderView::ShowPrompt(const std::string& promptText) {
    out_ << promptText;
}

void OrderView::ShowSubmitOrderResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "주문이 접수되었습니다: " << orderNumber << " ("
             << ColorizeStatus(OrderStatus::Reserved, OrderStatusToString(OrderStatus::Reserved)) << ")\n";
    } else {
        out_ << "오류: " << errorMessage << "\n";
    }
}

void OrderView::ShowPendingApprovals(const std::vector<Order>& orders) {
    WriteHeader("승인 대기 목록 조회");
    out_ << "주문번호 | 고객 | 시료 | 수량\n";
    for (const Order& order : orders) {
        out_ << order.orderNumber << " | " << order.customerName << " | " << order.sampleId << " | "
             << order.quantity << "\n";
    }
}

void OrderView::ShowNoPendingApprovals() {
    WriteHeader("승인 대기 목록 조회");
    out_ << "승인 대기 중인 주문이 없습니다.\n";
}

void OrderView::ShowInvalidApprovalAction() {
    out_ << "오류: 올바르지 않은 처리 방법입니다. A(승인) 또는 R(거절)을 입력하세요.\n";
}

void OrderView::ShowApproveResult(bool success, const std::string& orderNumber, OrderStatus resultingStatus,
                                  const std::string& errorMessage) {
    if (success) {
        out_ << "주문 " << orderNumber << " 승인 완료: " << ColorizeStatus(resultingStatus, OrderStatusToString(resultingStatus))
             << "\n";
    } else {
        out_ << "오류: " << errorMessage << "\n";
    }
}

void OrderView::ShowRejectResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "주문 " << orderNumber << " 이(가) "
             << ColorizeStatus(OrderStatus::Rejected, OrderStatusToString(OrderStatus::Rejected)) << " 처리되었습니다.\n";
    } else {
        out_ << "오류: " << errorMessage << "\n";
    }
}

void OrderView::ShowReleaseResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "주문 " << orderNumber << " 이(가) "
             << ColorizeStatus(OrderStatus::Released, OrderStatusToString(OrderStatus::Released)) << " 처리되었습니다.\n";
    } else {
        out_ << "오류: " << errorMessage << "\n";
    }
}

void OrderView::ShowReleasableOrders(const std::vector<Order>& orders) {
    WriteHeader("출고 처리");
    out_ << "주문번호 | 고객 | 시료 | 수량 | 상태\n";
    for (const Order& order : orders) {
        out_ << order.orderNumber << " | " << order.customerName << " | " << order.sampleId << " | "
             << order.quantity << " | " << ColorizeStatus(order.status, OrderStatusToString(order.status)) << "\n";
    }
}

void OrderView::ShowNoReleasableOrders() {
    WriteHeader("출고 처리");
    out_ << "출고 가능한 주문이 없습니다.\n";
}
