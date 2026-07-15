#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "../Models/Order.h"

class OrderView {
public:
    explicit OrderView(std::ostream& out = std::cout);

    void ShowOrderMenu();
    void ShowInvalidMenuChoice();

    // Writes prompt text with no trailing newline, so the next line of input
    // (read separately by the controller via its own std::istream&) appears
    // to follow it on the same line, like SampleView::PromptLine's echo but
    // without owning the input stream itself.
    void ShowPrompt(const std::string& promptText);

    void ShowSubmitOrderResult(bool success, const std::string& orderNumber, const std::string& errorMessage);

    void ShowPendingApprovals(const std::vector<Order>& orders);
    void ShowNoPendingApprovals();

    void ShowInvalidApprovalAction();

    void ShowApproveResult(bool success, const std::string& orderNumber, OrderStatus resultingStatus,
                           const std::string& errorMessage);
    void ShowRejectResult(bool success, const std::string& orderNumber, const std::string& errorMessage);
    void ShowReleaseResult(bool success, const std::string& orderNumber, const std::string& errorMessage);

    // Shown before the release-order-number prompt so the user can see which
    // orders are actually eligible (status == CONFIRMED) before choosing one.
    void ShowReleasableOrders(const std::vector<Order>& orders);
    void ShowNoReleasableOrders();

private:
    void WriteHeader(const std::string& title) const;

    std::ostream& out_;
};
