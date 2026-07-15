#include "OrderView.h"

OrderView::OrderView(std::ostream& out) : out_(out) {}

void OrderView::ShowOrderMenu() {
    out_ << "Order Management\n"
         << "1) Submit new order\n"
         << "2) Review pending approvals (approve/reject)\n"
         << "3) Release a confirmed order\n"
         << "4) Back\n";
}

void OrderView::ShowInvalidMenuChoice() {
    out_ << "Invalid menu choice.\n";
}

void OrderView::ShowSubmitOrderResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "Order submitted: " << orderNumber << " (" << OrderStatusToString(OrderStatus::Reserved) << ")\n";
    } else {
        out_ << "Error: " << errorMessage << "\n";
    }
}

void OrderView::ShowPendingApprovals(const std::vector<Order>& orders) {
    out_ << "Pending Approvals\n";
    for (const Order& order : orders) {
        out_ << order.orderNumber << " | " << order.customerName << " | " << order.sampleId << " | "
             << order.quantity << "\n";
    }
}

void OrderView::ShowNoPendingApprovals() {
    out_ << "No pending approvals.\n";
}

void OrderView::ShowInvalidApprovalAction() {
    out_ << "Invalid approval action. Please enter A or R.\n";
}

void OrderView::ShowApproveResult(bool success, const std::string& orderNumber, OrderStatus resultingStatus,
                                  const std::string& errorMessage) {
    if (success) {
        out_ << "Order " << orderNumber << " approved: " << OrderStatusToString(resultingStatus) << "\n";
    } else {
        out_ << "Error: " << errorMessage << "\n";
    }
}

void OrderView::ShowRejectResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "Order " << orderNumber << " rejected.\n";
    } else {
        out_ << "Error: " << errorMessage << "\n";
    }
}

void OrderView::ShowReleaseResult(bool success, const std::string& orderNumber, const std::string& errorMessage) {
    if (success) {
        out_ << "Order " << orderNumber << " released.\n";
    } else {
        out_ << "Error: " << errorMessage << "\n";
    }
}
