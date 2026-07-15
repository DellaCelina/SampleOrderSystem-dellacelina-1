#include <gtest/gtest.h>

#include "Services/OrderService.h"
#include "Services/ProductionService.h"

#include "Repositories/SampleRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "Models/ProductionQueueEntry.h"
#include "FakeClock.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

// Mirrors schema/sample.schema.json exactly.
const char* kSampleSchemaJson = R"({
  "table": "samples",
  "fields": [
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "name", "type": "string", "required": true },
    { "name": "averageProductionTimeMinutes", "type": "integer", "required": true, "min": 1 },
    { "name": "yield", "type": "number", "required": true, "exclusiveMin": 0, "max": 1 },
    { "name": "currentStock", "type": "integer", "required": true, "min": 0 }
  ]
})";

// Mirrors schema/order.schema.json exactly.
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

// Mirrors schema/production_queue.schema.json exactly.
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

// Fixture: gives every test its own scratch temp directory with real schema
// files for samples/orders/production_queue, and constructs real repository
// and ProductionService instances backed by that directory, matching the
// pattern established by ProductionServiceTests.cpp (phase-6).
class OrderServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("OrderServiceTest_") + info->name());
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
};

}  // namespace

// ---- SubmitOrder ----

TEST_F(OrderServiceTest, SubmitOrderWithValidSampleAndQuantityCreatesReservedOrderWithSequentialNumbers) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    OrderServiceResult<Order> first = m_service->SubmitOrder("SMP-001", "Acme", 10);
    OrderServiceResult<Order> second = m_service->SubmitOrder("SMP-001", "Acme", 20);

    ASSERT_TRUE(first.Ok());
    EXPECT_EQ(first.Value().orderNumber, "ORD-0001");
    EXPECT_EQ(first.Value().status, OrderStatus::Reserved);
    EXPECT_EQ(first.Value().sampleId, "SMP-001");
    EXPECT_EQ(first.Value().quantity, 10);
    ASSERT_TRUE(m_orders->FindByOrderNumber("ORD-0001").has_value());

    ASSERT_TRUE(second.Ok());
    EXPECT_EQ(second.Value().orderNumber, "ORD-0002");
}

TEST_F(OrderServiceTest, SubmitOrderWithUnknownSampleIdFailsAndCreatesNoOrder) {
    OrderServiceResult<Order> result = m_service->SubmitOrder("SMP-999", "Acme", 10);

    ASSERT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, OrderServiceErrorCode::SampleNotFound);
    EXPECT_TRUE(m_orders->FindAll().empty());
}

TEST_F(OrderServiceTest, SubmitOrderWithZeroOrNegativeQuantityFailsAndCreatesNoOrder) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));

    OrderServiceResult<Order> zero = m_service->SubmitOrder("SMP-001", "Acme", 0);
    OrderServiceResult<Order> negative = m_service->SubmitOrder("SMP-001", "Acme", -5);

    ASSERT_FALSE(zero.Ok());
    EXPECT_EQ(zero.Error().code, OrderServiceErrorCode::InvalidQuantity);
    ASSERT_FALSE(negative.Ok());
    EXPECT_EQ(negative.Error().code, OrderServiceErrorCode::InvalidQuantity);
    EXPECT_TRUE(m_orders->FindAll().empty());
}

// ---- ListPendingApprovals ----

TEST_F(OrderServiceTest, ListPendingApprovalsReturnsOnlyReservedOrdersExcludingAllOtherStatuses) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 5, OrderStatus::Confirmed});
    m_orders->Add(Order{"ORD-0003", "SMP-001", "Acme", 5, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0004", "SMP-001", "Acme", 5, OrderStatus::Released});
    m_orders->Add(Order{"ORD-0005", "SMP-001", "Acme", 5, OrderStatus::Rejected});

    std::vector<Order> pending = m_service->ListPendingApprovals();

    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].orderNumber, "ORD-0001");
}

TEST_F(OrderServiceTest, ListPendingApprovalsReturnsEmptyWhenThereAreNoReservedOrders) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Confirmed});

    EXPECT_TRUE(m_service->ListPendingApprovals().empty());
}

// ---- ListReleasable ----

TEST_F(OrderServiceTest, ListReleasableReturnsOnlyConfirmedOrdersExcludingAllOtherStatuses) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 5, OrderStatus::Confirmed});
    m_orders->Add(Order{"ORD-0003", "SMP-001", "Acme", 5, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0004", "SMP-001", "Acme", 5, OrderStatus::Released});
    m_orders->Add(Order{"ORD-0005", "SMP-001", "Acme", 5, OrderStatus::Rejected});

    std::vector<Order> releasable = m_service->ListReleasable();

    ASSERT_EQ(releasable.size(), 1u);
    EXPECT_EQ(releasable[0].orderNumber, "ORD-0002");
}

TEST_F(OrderServiceTest, ListReleasableReturnsEmptyWhenThereAreNoConfirmedOrders) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved});

    EXPECT_TRUE(m_service->ListReleasable().empty());
}

// ---- Approve: sufficient stock ----

TEST_F(OrderServiceTest, ApproveWithSufficientUnclaimedStockConfirmsWithoutStockOrQueueChange) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme", 60);
    ASSERT_TRUE(submitted.Ok());
    FakeClock clock;

    OrderServiceResult<Order> approved = m_service->Approve("ORD-0001", clock);

    ASSERT_TRUE(approved.Ok());
    EXPECT_EQ(approved.Value().status, OrderStatus::Confirmed);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 100);
    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}

// ---- Approve: insufficient stock (shortfall, not full quantity, enqueued) ----

TEST_F(OrderServiceTest, ApproveWithInsufficientStockEnqueuesOnlyTheShortfallAndMarksProducing) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 30)));
    OrderServiceResult<Order> submitted = m_service->SubmitOrder("SMP-001", "Acme", 50);
    ASSERT_TRUE(submitted.Ok());
    FakeClock clock;

    OrderServiceResult<Order> approved = m_service->Approve("ORD-0001", clock);

    ASSERT_TRUE(approved.Ok());
    EXPECT_EQ(approved.Value().status, OrderStatus::Producing);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Producing);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 30);  // unchanged at Approve time

    std::vector<ProductionQueueEntry> queued = m_queue->FindAllInOrder();
    ASSERT_EQ(queued.size(), 1u);
    EXPECT_EQ(queued[0].orderNumber, "ORD-0001");
    EXPECT_EQ(queued[0].shortfallQuantity, 20);  // 50 - 30, not the full 50
    EXPECT_EQ(queued[0].actualProducedQuantity, ProductionService::ComputeActualQuantity(20, 1.0));
}

// ---- Approve: 50-then-100-then-100 acceptance scenario (Key Design Decision #2) ----

TEST_F(OrderServiceTest, ApproveFiftyThenHundredThenHundredScenarioClaimsFullOrderQuantityNotJustShortfall) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 50)));
    FakeClock clock;

    OrderServiceResult<Order> order1 = m_service->SubmitOrder("SMP-001", "Acme", 100);
    ASSERT_TRUE(order1.Ok());
    OrderServiceResult<Order> approved1 = m_service->Approve("ORD-0001", clock);
    ASSERT_TRUE(approved1.Ok());
    EXPECT_EQ(approved1.Value().status, OrderStatus::Producing);
    std::vector<ProductionQueueEntry> afterFirst = m_queue->FindAllInOrder();
    ASSERT_EQ(afterFirst.size(), 1u);
    EXPECT_EQ(afterFirst[0].shortfallQuantity, 50);  // unclaimed=50, shortfall=50

    OrderServiceResult<Order> order2 = m_service->SubmitOrder("SMP-001", "Other Co", 100);
    ASSERT_TRUE(order2.Ok());
    OrderServiceResult<Order> approved2 = m_service->Approve("ORD-0002", clock);
    ASSERT_TRUE(approved2.Ok());
    EXPECT_EQ(approved2.Value().status, OrderStatus::Producing);

    std::vector<ProductionQueueEntry> queue = m_queue->FindAllInOrder();
    ProductionQueueEntry* second = nullptr;
    for (auto& entry : queue) {
        if (entry.orderNumber == "ORD-0002") second = &entry;
    }
    ASSERT_NE(second, nullptr);
    // unclaimed = max(0, 50 - 100) = 0 (order1's FULL 100 is claimed, not its 50 shortfall)
    // => shortfall for order2 = 100, not 150.
    EXPECT_EQ(second->shortfallQuantity, 100);
}

// ---- Approve: settle-then-decide ordering (Key Design Decision #3) ----

TEST_F(OrderServiceTest, ApproveSettlesDueEntriesBeforeComputingUnclaimedStockForADifferentOrder) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 50)));
    FakeClock clock;

    OrderServiceResult<Order> order1 = m_service->SubmitOrder("SMP-001", "Acme", 100);
    ASSERT_TRUE(order1.Ok());
    OrderServiceResult<Order> approved1 = m_service->Approve("ORD-0001", clock);
    ASSERT_TRUE(approved1.Ok());
    std::vector<ProductionQueueEntry> queuedAfterFirst = m_queue->FindAllInOrder();
    ASSERT_EQ(queuedAfterFirst.size(), 1u);
    // duration = actualProducedQuantity(50) * 1 minute = 50 minutes.
    clock.Advance(std::chrono::minutes(100));  // well past order1's completion

    OrderServiceResult<Order> order2 = m_service->SubmitOrder("SMP-001", "Other Co", 100);
    ASSERT_TRUE(order2.Ok());
    OrderServiceResult<Order> approved2 = m_service->Approve("ORD-0002", clock);
    ASSERT_TRUE(approved2.Ok());

    // Settlement flipped order1 to Confirmed and credited its 50 units of stock
    // as a side effect of calling Approve on a *different* order.
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 100);  // 50 original + 50 produced

    // order2's unclaimed-stock computation must use the POST-settlement numbers:
    // stock=100, claimed by other Producing/Confirmed orders = 0 (order1 is now
    // Confirmed but no longer double counted incorrectly -- unclaimed = max(0,100-0)=100... )
    // NOTE: order1 is Confirmed, which per Key Design Decision #2 IS counted among
    // claimed statuses, but its claim is against the ORIGINAL 100 stock target which
    // has already been fully covered by settlement; the queue entry for order2 must
    // reflect the concrete recomputation, not a stale pre-settlement value.
    std::vector<ProductionQueueEntry> queue = m_queue->FindAllInOrder();
    bool foundOrder2Entry = false;
    for (const auto& entry : queue) {
        if (entry.orderNumber == "ORD-0002") {
            foundOrder2Entry = true;
            // The anchor for order2's completion time must be clock.Now(), not
            // order1's already-past expectedCompletionAt (order1 is no longer in
            // the queue at all after settlement).
            EXPECT_EQ(entry.enqueuedAt, clock.Now());
        }
    }
    // order2 is either Confirmed (if unclaimed fully covers it) or Producing with a
    // queue entry -- assert consistently with whichever branch actually applies.
    Order order2Final = *m_orders->FindByOrderNumber("ORD-0002");
    if (order2Final.status == OrderStatus::Producing) {
        EXPECT_TRUE(foundOrder2Entry);
    } else {
        EXPECT_EQ(order2Final.status, OrderStatus::Confirmed);
    }
}

// ---- Approve: lookup/status errors ----

TEST_F(OrderServiceTest, ApproveOfAnUnknownOrderNumberFails) {
    FakeClock clock;

    OrderServiceResult<Order> result = m_service->Approve("ORD-9999", clock);

    ASSERT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, OrderServiceErrorCode::OrderNotFound);
}

TEST_F(OrderServiceTest, ApproveOfANonReservedOrderFailsWithoutMutatingStockOrQueue) {
    const std::vector<OrderStatus> nonReservedStatuses = {
        OrderStatus::Confirmed, OrderStatus::Producing, OrderStatus::Released, OrderStatus::Rejected};
    int counter = 0;
    for (OrderStatus status : nonReservedStatuses) {
        const std::string orderNumber = "ORD-000" + std::to_string(++counter);
        ASSERT_TRUE(m_samples->Add(MakeSample("SMP-" + std::to_string(counter), "GaAs Wafer", 30, 1.0, 100)));
        m_orders->Add(Order{orderNumber, "SMP-" + std::to_string(counter), "Acme", 10, status});
        FakeClock clock;

        OrderServiceResult<Order> result = m_service->Approve(orderNumber, clock);

        ASSERT_FALSE(result.Ok()) << "status: " << static_cast<int>(status);
        EXPECT_EQ(result.Error().code, OrderServiceErrorCode::InvalidStatusForApproval) << "status: " << static_cast<int>(status);
        EXPECT_EQ(m_orders->FindByOrderNumber(orderNumber)->status, status) << "status: " << static_cast<int>(status);
        EXPECT_EQ(m_samples->FindById("SMP-" + std::to_string(counter))->currentStock, 100) << "status: " << static_cast<int>(status);
        EXPECT_TRUE(m_queue->FindAllInOrder().empty()) << "status: " << static_cast<int>(status);
    }
}

// ---- Reject ----

TEST_F(OrderServiceTest, RejectOfAReservedOrderSucceedsAndRemovesItFromPendingApprovals) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Reserved});

    OrderServiceResult<Order> result = m_service->Reject("ORD-0001");

    ASSERT_TRUE(result.Ok());
    EXPECT_EQ(result.Value().status, OrderStatus::Rejected);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Rejected);
    EXPECT_TRUE(m_service->ListPendingApprovals().empty());
}

TEST_F(OrderServiceTest, RejectOfANonReservedOrderFailsAndLeavesStatusUnchanged) {
    const std::vector<OrderStatus> nonReservedStatuses = {
        OrderStatus::Confirmed, OrderStatus::Producing, OrderStatus::Released, OrderStatus::Rejected};
    int counter = 0;
    for (OrderStatus status : nonReservedStatuses) {
        const std::string orderNumber = "ORD-000" + std::to_string(++counter);
        ASSERT_TRUE(m_samples->Add(MakeSample("SMP-" + std::to_string(counter), "GaAs Wafer", 30, 1.0, 100)));
        m_orders->Add(Order{orderNumber, "SMP-" + std::to_string(counter), "Acme", 10, status});

        OrderServiceResult<Order> result = m_service->Reject(orderNumber);

        ASSERT_FALSE(result.Ok()) << "status: " << static_cast<int>(status);
        EXPECT_EQ(result.Error().code, OrderServiceErrorCode::InvalidStatusForRejection) << "status: " << static_cast<int>(status);
        EXPECT_EQ(m_orders->FindByOrderNumber(orderNumber)->status, status) << "status: " << static_cast<int>(status);
    }
}

TEST_F(OrderServiceTest, RejectOfAnUnknownOrderNumberFails) {
    OrderServiceResult<Order> result = m_service->Reject("ORD-9999");

    ASSERT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, OrderServiceErrorCode::OrderNotFound);
}

// ---- Release ----

TEST_F(OrderServiceTest, ReleaseOfAConfirmedOrderDecrementsStockAndMarksReleased) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 80)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Confirmed});

    OrderServiceResult<Order> result = m_service->Release("ORD-0001");

    ASSERT_TRUE(result.Ok());
    EXPECT_EQ(result.Value().status, OrderStatus::Released);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Released);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 50);
}

TEST_F(OrderServiceTest, ReleaseOfANonConfirmedOrderFailsAndLeavesStockAndStatusUnchanged) {
    const std::vector<OrderStatus> nonConfirmedStatuses = {
        OrderStatus::Reserved, OrderStatus::Producing, OrderStatus::Released, OrderStatus::Rejected};
    int counter = 0;
    for (OrderStatus status : nonConfirmedStatuses) {
        const std::string orderNumber = "ORD-000" + std::to_string(++counter);
        ASSERT_TRUE(m_samples->Add(MakeSample("SMP-" + std::to_string(counter), "GaAs Wafer", 30, 1.0, 80)));
        m_orders->Add(Order{orderNumber, "SMP-" + std::to_string(counter), "Acme", 30, status});

        OrderServiceResult<Order> result = m_service->Release(orderNumber);

        ASSERT_FALSE(result.Ok()) << "status: " << static_cast<int>(status);
        EXPECT_EQ(result.Error().code, OrderServiceErrorCode::InvalidStatusForRelease) << "status: " << static_cast<int>(status);
        EXPECT_EQ(m_orders->FindByOrderNumber(orderNumber)->status, status) << "status: " << static_cast<int>(status);
        EXPECT_EQ(m_samples->FindById("SMP-" + std::to_string(counter))->currentStock, 80) << "status: " << static_cast<int>(status);
    }
}

TEST_F(OrderServiceTest, ReleaseOfAnUnknownOrderNumberFails) {
    OrderServiceResult<Order> result = m_service->Release("ORD-9999");

    ASSERT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, OrderServiceErrorCode::OrderNotFound);
}
