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

    void ShowSubmitOrderResult(bool success, const std::string& orderNumber, const std::string& errorMessage);

    void ShowPendingApprovals(const std::vector<Order>& orders);
    void ShowNoPendingApprovals();

    void ShowInvalidApprovalAction();

    void ShowApproveResult(bool success, const std::string& orderNumber, OrderStatus resultingStatus,
                           const std::string& errorMessage);
    void ShowRejectResult(bool success, const std::string& orderNumber, const std::string& errorMessage);
    void ShowReleaseResult(bool success, const std::string& orderNumber, const std::string& errorMessage);

private:
    std::ostream& out_;
};
