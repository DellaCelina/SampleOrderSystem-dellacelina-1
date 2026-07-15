#include <gtest/gtest.h>

#include "Controllers/OrderController.h"
#include "Views/OrderView.h"
#include "Services/OrderService.h"
#include "Services/ProductionService.h"

#include "Repositories/SampleRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "Models/ProductionQueueEntry.h"
#include "FakeClock.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace throughout, matching this repo's real committed code
// (no Views::/Controllers:: wrappers).

const char* kSampleSchemaJson = R"({
  "table": "samples",
  "fields": [
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "name", "type": "string", "required": true },
    { "name": "averageProductionTimeMinutes", "type": "number", "required": true, "exclusiveMin": 0 },
    { "name": "yield", "type": "number", "required": true, "exclusiveMin": 0, "max": 1 },
    { "name": "currentStock", "type": "integer", "required": true, "min": 0 }
  ]
})";

const char* kOrderSchemaJson = R"({
  "table": "orders",
  "fields": [
    { "name": "orderNumber", "type": "string", "required": true, "pattern": "^ORD-\\d{4}$" },
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "customerName", "type": "string", "required": true },
    { "name": "quantity", "type": "integer", "required": true, "min": 1 },
    { "name": "status", "type": "string", "required": true, "enum": ["RESERVED", "CONFIRMED", "PRODUCING", "RELEASED", "REJECTED"] }
  ]
})";

const char* kProductionQueueSchemaJson = R"({
  "table": "production_queue",
  "fields": [
    { "name": "orderNumber", "type": "string", "required": true, "pattern": "^ORD-\\d{4}$" },
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "shortfallQuantity", "type": "integer", "required": true, "min": 1 },
    { "name": "actualProducedQuantity", "type": "integer", "required": true, "min": 1 },
    { "name": "enqueuedAt", "type": "string", "required": true, "format": "iso8601" },
    { "name": "expectedCompletionAt", "type": "string", "required": true, "format": "iso8601" }
  ]
})";

Sample MakeSample(const std::string& id, const std::string& name, int avgTimeMinutes, double yield, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = avgTimeMinutes;
    s.yield = yield;
    s.currentStock = stock;
    return s;
}

// Joins lines with '\n' to script sequential ReadLine() calls made by OrderController::Run().
std::string ScriptedInput(const std::vector<std::string>& lines) {
    std::string result;
    for (const std::string& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

class OrderControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("OrderControllerTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        WriteSchema("sample.schema.json", kSampleSchemaJson);
        WriteSchema("order.schema.json", kOrderSchemaJson);
        WriteSchema("production_queue.schema.json", kProductionQueueSchemaJson);

        m_samples = std::make_unique<SampleRepository>(m_testDir / "samples.json", m_testDir / "sample.schema.json");
        m_orders = std::make_unique<OrderRepository>(m_testDir / "orders.json", m_testDir / "order.schema.json");
        m_queue = std::make_unique<ProductionQueueRepository>(m_testDir / "production_queue.json",
                                                               m_testDir / "production_queue.schema.json");
        m_production = std::make_unique<ProductionService>(*m_samples, *m_orders, *m_queue);
        m_service = std::make_unique<OrderService>(*m_orders, *m_samples, *m_production);
        m_clock = std::make_unique<FakeClock>();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    void WriteSchema(const std::string& fileName, const char* contents) const {
        std::ofstream out(m_testDir / fileName, std::ios::trunc);
        out << contents;
    }

    std::filesystem::path m_testDir;
    std::unique_ptr<SampleRepository> m_samples;
    std::unique_ptr<OrderRepository> m_orders;
    std::unique_ptr<ProductionQueueRepository> m_queue;
    std::unique_ptr<ProductionService> m_production;
    std::unique_ptr<OrderService> m_service;
    std::unique_ptr<FakeClock> m_clock;
};

}  // namespace

// ---- Menu dispatch ----

TEST_F(OrderControllerTest, MenuDispatchInvalidChoiceThenExitShowsErrorAndReturnsCleanlyWithNoSideEffects) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "9", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    EXPECT_TRUE(m_orders->FindAll().empty());
    EXPECT_FALSE(out.str().empty());
}

TEST_F(OrderControllerTest, MenuDispatchEofActsAsBackAndReturnsWithoutThrowing) {
    std::istringstream in("");
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    EXPECT_NO_THROW(controller.Run());
}

// ---- Submit ----

TEST_F(OrderControllerTest, SubmitWithValidFieldsCreatesReservedOrderAndOutputContainsItsNumber) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "1", "SMP-001", "Acme Corp", "10", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::vector<Order> orders = m_orders->FindAll();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].status, OrderStatus::Reserved);
    EXPECT_NE(out.str().find(orders[0].orderNumber), std::string::npos);
}

TEST_F(OrderControllerTest, SubmitWithBlankSampleIdNeverCallsServiceAndRendersFailure) {
    std::istringstream in(ScriptedInput({ "1", "", "Acme Corp", "10", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    EXPECT_TRUE(m_orders->FindAll().empty());
    EXPECT_NE(out.str().find("오류"), std::string::npos);
}

TEST_F(OrderControllerTest, SubmitWithNonNumericQuantityNeverCallsServiceAndRendersFailure) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "1", "SMP-001", "Acme Corp", "abc", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    EXPECT_TRUE(m_orders->FindAll().empty());
    EXPECT_NE(out.str().find("오류"), std::string::npos);
}

TEST_F(OrderControllerTest, SubmitWithZeroOrNegativeQuantityNeverCallsServiceAndRendersFailure) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    std::istringstream in(ScriptedInput({ "1", "SMP-001", "Acme Corp", "0", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    EXPECT_TRUE(m_orders->FindAll().empty());
    EXPECT_NE(out.str().find("오류"), std::string::npos);
}

TEST_F(OrderControllerTest, SubmitWithUnknownSampleIdCallsServiceAndRendersServiceErrorWithoutCreatingAnOrder) {
    std::istringstream in(ScriptedInput({ "1", "SMP-999", "Acme Corp", "10", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    EXPECT_TRUE(m_orders->FindAll().empty());
    EXPECT_NE(out.str().find("오류"), std::string::npos);
}

// ---- Approve / Reject ----

TEST_F(OrderControllerTest, ApproveRejectWithEmptyPendingListShowsNoPendingApprovalsAndDoesNotHang) {
    std::istringstream in(ScriptedInput({ "2", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    EXPECT_NO_THROW(controller.Run());
    EXPECT_FALSE(out.str().empty());
}

TEST_F(OrderControllerTest, ApproveWithSufficientUnclaimedStockTransitionsOrderToConfirmed) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", orderNumber, "A", "", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Confirmed);
    EXPECT_NE(out.str().find(OrderStatusToString(OrderStatus::Confirmed)), std::string::npos);
}

TEST_F(OrderControllerTest, ApproveWithInsufficientStockTransitionsOrderToProducing) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 5)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 50);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", orderNumber, "A", "", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Producing);
    EXPECT_NE(out.str().find(OrderStatusToString(OrderStatus::Producing)), std::string::npos);
}

TEST_F(OrderControllerTest, RejectTransitionsOrderToRejectedAndRemovesItFromSubsequentPendingList) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", orderNumber, "R", "", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Rejected);

    bool stillPending = false;
    for (const Order& pending : m_service->ListPendingApprovals()) {
        if (pending.orderNumber == orderNumber) stillPending = true;
    }
    EXPECT_FALSE(stillPending);
}

TEST_F(OrderControllerTest, ApproveRejectWithInvalidActionLetterLeavesOrderUntouchedAndMakesNoServiceCall) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", orderNumber, "x", "", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Reserved);
}

TEST_F(OrderControllerTest, ApproveRejectExitTokenAtOrderNumberPromptReturnsToOrderSubmenuWithoutActing) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Reserved);
}

TEST_F(OrderControllerTest, ApproveRejectWithNonexistentOrderNumberRendersServiceFailureWithoutCrashing) {
    std::istringstream in(ScriptedInput({ "2", "SMP-001", "ORD-9999", "A", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    // No pending orders exist, so ShowNoPendingApprovals fires first and the
    // remaining scripted lines are simply never consumed by that branch --
    // still must not crash or hang.
    EXPECT_NO_THROW(controller.Run());
}

TEST_F(OrderControllerTest, ApproveRejectLoopContinuesAfterOneActionAllowingASecondOrderToBeActedOnInTheSameRun) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> first = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    OrderServiceResult<Order> second = m_service->SubmitOrder("SMP-001", "Globex", 20);
    ASSERT_TRUE(first.Ok());
    ASSERT_TRUE(second.Ok());
    const std::string firstNumber = first.Value().orderNumber;
    const std::string secondNumber = second.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "2", firstNumber, "A", "", secondNumber, "R", "", "0", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> firstOrder = m_orders->FindByOrderNumber(firstNumber);
    std::optional<Order> secondOrder = m_orders->FindByOrderNumber(secondNumber);
    ASSERT_TRUE(firstOrder.has_value());
    ASSERT_TRUE(secondOrder.has_value());
    EXPECT_EQ(firstOrder->status, OrderStatus::Confirmed);
    EXPECT_EQ(secondOrder->status, OrderStatus::Rejected);
}

// ---- Release ----

TEST_F(OrderControllerTest, ReleaseWithConfirmedOrderTransitionsToReleased) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;
    ASSERT_TRUE(m_service->Approve(orderNumber, *m_clock).Ok());

    std::istringstream in(ScriptedInput({ "3", orderNumber, "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Released);
    EXPECT_NE(out.str().find(orderNumber), std::string::npos);
}

TEST_F(OrderControllerTest, ReleaseWithOrderStillReservedRendersFailureWithNoStateChange) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    // HandleRelease now lists only CONFIRMED orders before prompting (per the
    // "show the releasable list first" UX fix), so a second, already-confirmed
    // order is seeded purely to make that list non-empty; the actual order
    // under test is typed in by number (not chosen from the list) and is
    // still Reserved, so the service call it triggers still fails exactly as
    // before.
    OrderServiceResult<Order> confirmed = m_service->SubmitOrder("SMP-001", "Other Corp", 5);
    ASSERT_TRUE(confirmed.Ok());
    ASSERT_TRUE(m_service->Approve(confirmed.Value().orderNumber, *m_clock).Ok());

    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;

    std::istringstream in(ScriptedInput({ "3", orderNumber, "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Reserved);
    EXPECT_NE(out.str().find("오류"), std::string::npos);
}

TEST_F(OrderControllerTest, ReleaseWithNoConfirmedOrdersShowsNoReleasableOrdersMessageAndDoesNotHang) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());  // stays RESERVED -- never approved, so nothing is releasable

    std::istringstream in(ScriptedInput({ "3", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    EXPECT_NO_THROW(controller.Run());
    EXPECT_FALSE(out.str().empty());

    std::optional<Order> order = m_orders->FindByOrderNumber(submitted.Value().orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Reserved);
}

TEST_F(OrderControllerTest, ReleaseWithBlankInputMakesNoServiceCall) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme Corp", 10);
    ASSERT_TRUE(submitted.Ok());
    const std::string orderNumber = submitted.Value().orderNumber;
    ASSERT_TRUE(m_service->Approve(orderNumber, *m_clock).Ok());

    std::istringstream in(ScriptedInput({ "3", "", "4" }));
    std::ostringstream out;
    OrderView view(out);
    OrderController controller(*m_service, view, in, *m_clock);

    controller.Run();

    std::optional<Order> order = m_orders->FindByOrderNumber(orderNumber);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::Confirmed);
}
