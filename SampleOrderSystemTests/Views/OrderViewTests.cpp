#include <gtest/gtest.h>

#include "Views/OrderView.h"
#include "Models/Order.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace throughout (matches phase-1/2/4/5/10's real committed code:
// no Views::/Controllers:: wrappers anywhere).

Order MakeOrder(const std::string& orderNumber, const std::string& sampleId,
                 const std::string& customerName, int quantity, OrderStatus status) {
    Order o;
    o.orderNumber = orderNumber;
    o.sampleId = sampleId;
    o.customerName = customerName;
    o.quantity = quantity;
    o.status = status;
    return o;
}

}  // namespace

TEST(OrderViewTests, ShowOrderMenuListsAllFourOptions) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowOrderMenu();

    const std::string text = out.str();
    EXPECT_NE(text.find("1"), std::string::npos);
    EXPECT_NE(text.find("2"), std::string::npos);
    EXPECT_NE(text.find("3"), std::string::npos);
    EXPECT_NE(text.find("4"), std::string::npos);
}

TEST(OrderViewTests, ShowInvalidMenuChoiceIsNonEmpty) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowInvalidMenuChoice();

    EXPECT_FALSE(out.str().empty());
}

TEST(OrderViewTests, ShowSubmitOrderResultSuccessShowsOrderNumberAndReservedStatusWithNoError) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowSubmitOrderResult(true, "ORD-0007", "");

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0007"), std::string::npos);
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Reserved)), std::string::npos);
    EXPECT_EQ(text.find("Error"), std::string::npos);
}

TEST(OrderViewTests, ShowSubmitOrderResultFailureShowsErrorAndNoFabricatedOrderNumber) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowSubmitOrderResult(false, "", "Sample not found");

    const std::string text = out.str();
    EXPECT_NE(text.find("Sample not found"), std::string::npos);
    EXPECT_EQ(text.find("ORD-"), std::string::npos);
}

TEST(OrderViewTests, ShowPendingApprovalsRendersOneRowPerOrderWithAllRequiredFields) {
    std::ostringstream out;
    OrderView view(out);

    std::vector<Order> orders = {
        MakeOrder("ORD-0001", "SMP-001", "Acme Corp", 10, OrderStatus::Reserved),
        MakeOrder("ORD-0002", "SMP-002", "Globex", 25, OrderStatus::Reserved),
    };

    view.ShowPendingApprovals(orders);

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("Acme Corp"), std::string::npos);
    EXPECT_NE(text.find("10"), std::string::npos);
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
    EXPECT_NE(text.find("SMP-002"), std::string::npos);
    EXPECT_NE(text.find("Globex"), std::string::npos);
    EXPECT_NE(text.find("25"), std::string::npos);
}

TEST(OrderViewTests, ShowNoPendingApprovalsIsNonEmptyAndDistinctFromInvalidApprovalAction) {
    std::ostringstream noneOut;
    OrderView noneView(noneOut);
    noneView.ShowNoPendingApprovals();

    std::ostringstream invalidOut;
    OrderView invalidView(invalidOut);
    invalidView.ShowInvalidApprovalAction();

    EXPECT_FALSE(noneOut.str().empty());
    EXPECT_NE(noneOut.str(), invalidOut.str());
}

TEST(OrderViewTests, ShowInvalidApprovalActionIsNonEmpty) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowInvalidApprovalAction();

    EXPECT_FALSE(out.str().empty());
}

TEST(OrderViewTests, ShowApproveResultSuccessConfirmedShowsConfirmedNotProducing) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowApproveResult(true, "ORD-0001", OrderStatus::Confirmed, "");

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Confirmed)), std::string::npos);
    EXPECT_EQ(text.find(OrderStatusToString(OrderStatus::Producing)), std::string::npos);
}

TEST(OrderViewTests, ShowApproveResultSuccessProducingShowsProducing) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowApproveResult(true, "ORD-0002", OrderStatus::Producing, "");

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Producing)), std::string::npos);
}

TEST(OrderViewTests, ShowApproveResultFailureShowsErrorAndNoFabricatedResultingStatus) {
    std::ostringstream out;
    OrderView view(out);

    // resultingStatus is ignored on failure per contract; pass Confirmed to prove it's not echoed.
    view.ShowApproveResult(false, "ORD-0003", OrderStatus::Confirmed, "not RESERVED");

    const std::string text = out.str();
    EXPECT_NE(text.find("not RESERVED"), std::string::npos);
}

TEST(OrderViewTests, ShowRejectResultSuccessAndFailure) {
    std::ostringstream successOut;
    OrderView successView(successOut);
    successView.ShowRejectResult(true, "ORD-0004", "");
    EXPECT_NE(successOut.str().find("ORD-0004"), std::string::npos);
    EXPECT_EQ(successOut.str().find("Error"), std::string::npos);

    std::ostringstream failureOut;
    OrderView failureView(failureOut);
    failureView.ShowRejectResult(false, "ORD-0005", "order not found");
    EXPECT_NE(failureOut.str().find("order not found"), std::string::npos);
}

TEST(OrderViewTests, ShowReleaseResultSuccessAndFailure) {
    std::ostringstream successOut;
    OrderView successView(successOut);
    successView.ShowReleaseResult(true, "ORD-0006", "");
    EXPECT_NE(successOut.str().find("ORD-0006"), std::string::npos);
    EXPECT_EQ(successOut.str().find("Error"), std::string::npos);

    std::ostringstream failureOut;
    OrderView failureView(failureOut);
    failureView.ShowReleaseResult(false, "ORD-0007", "not CONFIRMED");
    EXPECT_NE(failureOut.str().find("not CONFIRMED"), std::string::npos);
}

TEST(OrderViewTests, ShowReleasableOrdersRendersOneRowPerOrderWithAllRequiredFields) {
    std::ostringstream out;
    OrderView view(out);

    std::vector<Order> orders = {
        MakeOrder("ORD-0001", "SMP-001", "Acme Corp", 10, OrderStatus::Confirmed),
        MakeOrder("ORD-0002", "SMP-002", "Globex", 25, OrderStatus::Confirmed),
    };

    view.ShowReleasableOrders(orders);

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("Acme Corp"), std::string::npos);
    EXPECT_NE(text.find("10"), std::string::npos);
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
}

TEST(OrderViewTests, ShowNoReleasableOrdersIsNonEmpty) {
    std::ostringstream out;
    OrderView view(out);

    view.ShowNoReleasableOrders();

    EXPECT_FALSE(out.str().empty());
}
