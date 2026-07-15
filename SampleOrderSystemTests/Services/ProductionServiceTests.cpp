#include <gtest/gtest.h>

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
#include <stdexcept>
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

std::chrono::system_clock::time_point MakeUtcSeconds(long long seconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}

Sample MakeSample(const std::string& id, const std::string& name, int avgTimeMinutes, double yield, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = avgTimeMinutes;
    s.yield = yield;
    s.currentStock = stock;
    return s;
}

Order MakeOrder(const std::string& orderNumber, const std::string& sampleId, const std::string& customerName,
                 int quantity, OrderStatus status) {
    Order o;
    o.orderNumber = orderNumber;
    o.sampleId = sampleId;
    o.customerName = customerName;
    o.quantity = quantity;
    o.status = status;
    return o;
}

// Fixture: gives every test its own scratch temp directory with real schema
// files for samples/orders/production_queue, and constructs real repository
// instances backed by that directory (per phase-6 DETAIL.md's instruction to
// test ProductionService against real repositories, not hand-rolled mocks).
class ProductionServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("ProductionServiceTest_") + info->name());
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
};

}  // namespace

// ---- ComputeShortfall (pure) ----

TEST(ProductionServiceMathTest, ComputeShortfallReturnsZeroWhenUnclaimedStockCoversTheRequestedQuantity) {
    EXPECT_EQ(ProductionService::ComputeShortfall(50, 50), 0);
    EXPECT_EQ(ProductionService::ComputeShortfall(50, 60), 0);
}

TEST(ProductionServiceMathTest, ComputeShortfallReturnsExactDifferenceWhenUnclaimedStockIsInsufficient) {
    EXPECT_EQ(ProductionService::ComputeShortfall(100, 40), 60);
}

TEST(ProductionServiceMathTest, ComputeShortfallEqualsRequestedQuantityWhenUnclaimedStockIsZero) {
    EXPECT_EQ(ProductionService::ComputeShortfall(30, 0), 30);
}

// ---- ComputeActualQuantity (pure, ceil(shortfall/yield)) ----

TEST(ProductionServiceMathTest, ComputeActualQuantityOfZeroShortfallIsZero) {
    EXPECT_EQ(ProductionService::ComputeActualQuantity(0, 0.9), 0);
}

TEST(ProductionServiceMathTest, ComputeActualQuantityWithYieldOneEqualsShortfallExactly) {
    EXPECT_EQ(ProductionService::ComputeActualQuantity(42, 1.0), 42);
}

TEST(ProductionServiceMathTest, ComputeActualQuantityWithExactDivisionYieldsNoRoundingUp) {
    EXPECT_EQ(ProductionService::ComputeActualQuantity(90, 0.9), 100);
}

TEST(ProductionServiceMathTest, ComputeActualQuantityRoundsUpNotDownOrTruncated) {
    // 10 / 0.9 = 11.11... => ceil => 12, confirms rounding up not truncation.
    EXPECT_EQ(ProductionService::ComputeActualQuantity(10, 0.9), 12);
}

TEST(ProductionServiceMathTest, ComputeActualQuantityWithLowYieldRequiresManyMoreUnitsProduced) {
    EXPECT_EQ(ProductionService::ComputeActualQuantity(1, 0.1), 10);
}

// ---- ComputeProductionDurationMinutes (pure) ----

TEST(ProductionServiceMathTest, ComputeProductionDurationMinutesMultipliesQuantityByAverageTime) {
    EXPECT_EQ(ProductionService::ComputeProductionDurationMinutes(12, 30), 360);
    EXPECT_EQ(ProductionService::ComputeProductionDurationMinutes(1, 45), 45);
}

TEST(ProductionServiceMathTest, ComputeProductionDurationMinutesIsZeroWhenNoUnitsAreProduced) {
    EXPECT_EQ(ProductionService::ComputeProductionDurationMinutes(0, 30), 0);
}

// ---- ComputeCompletionTime (pure, FIFO max-chain) ----

TEST(ProductionServiceMathTest, ComputeCompletionTimeWithEmptyQueueIsEnqueuedAtPlusDuration) {
    std::chrono::system_clock::time_point enqueuedAt = MakeUtcSeconds(1000);

    std::chrono::system_clock::time_point result =
        ProductionService::ComputeCompletionTime(enqueuedAt, std::nullopt, 30);

    EXPECT_EQ(result, enqueuedAt + std::chrono::minutes(30));
}

TEST(ProductionServiceMathTest, ComputeCompletionTimeChainsOffAPreviousTailCompletionThatIsLater) {
    std::chrono::system_clock::time_point enqueuedAt = MakeUtcSeconds(1000);
    std::chrono::system_clock::time_point previousTail = MakeUtcSeconds(5000);

    std::chrono::system_clock::time_point result =
        ProductionService::ComputeCompletionTime(enqueuedAt, previousTail, 30);

    EXPECT_EQ(result, previousTail + std::chrono::minutes(30));
}

TEST(ProductionServiceMathTest, ComputeCompletionTimeIgnoresAStalePreviousTailCompletionEarlierThanEnqueuedAt) {
    std::chrono::system_clock::time_point enqueuedAt = MakeUtcSeconds(5000);
    std::chrono::system_clock::time_point previousTail = MakeUtcSeconds(1000);

    std::chrono::system_clock::time_point result =
        ProductionService::ComputeCompletionTime(enqueuedAt, previousTail, 30);

    EXPECT_EQ(result, enqueuedAt + std::chrono::minutes(30));
}

TEST(ProductionServiceMathTest, ComputeCompletionTimeWithPreviousTailEqualToEnqueuedAtIsBoundaryConsistent) {
    std::chrono::system_clock::time_point enqueuedAt = MakeUtcSeconds(2000);

    std::chrono::system_clock::time_point result =
        ProductionService::ComputeCompletionTime(enqueuedAt, enqueuedAt, 30);

    EXPECT_EQ(result, enqueuedAt + std::chrono::minutes(30));
}

// ---- Enqueue ----

TEST_F(ProductionServiceTest, EnqueueAgainstAnEmptyQueueAnchorsCompletionTimeOnNow) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    ProductionQueueEntry entry = service.Enqueue("ORD-0001", "SMP-001", /*shortfallQuantity=*/10, clock);

    // actualProducedQuantity = ceil(10/0.9) = 12; duration = 12*30 = 360 minutes.
    EXPECT_EQ(entry.actualProducedQuantity, 12);
    EXPECT_EQ(entry.enqueuedAt, clock.Now());
    EXPECT_EQ(entry.expectedCompletionAt, clock.Now() + std::chrono::minutes(360));
}

TEST_F(ProductionServiceTest, EnqueueingASecondEntryChainsOffTheFirstEntrysCompletionTimeRatherThanNow) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 100, OrderStatus::Producing));
    m_orders->Add(MakeOrder("ORD-0002", "SMP-001", "Acme", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    ProductionQueueEntry first = service.Enqueue("ORD-0001", "SMP-001", 30, clock);
    ProductionQueueEntry second = service.Enqueue("ORD-0002", "SMP-001", 10, clock);

    // second.enqueuedAt is still "now" (clock hasn't advanced), but its
    // completion must chain off first's completion, not off now + its own duration.
    EXPECT_EQ(second.enqueuedAt, clock.Now());
    EXPECT_EQ(second.expectedCompletionAt, first.expectedCompletionAt + std::chrono::minutes(10 * 30));
    EXPECT_GT(second.expectedCompletionAt, clock.Now() + std::chrono::minutes(10 * 30));
}

TEST_F(ProductionServiceTest, EnqueuedEntryFieldsRoundTripThroughTheProductionQueueRepository) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 0.9, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    ProductionQueueEntry created = service.Enqueue("ORD-0001", "SMP-001", 10, clock);

    std::optional<ProductionQueueEntry> reread = m_queue->PeekHead();
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->orderNumber, created.orderNumber);
    EXPECT_EQ(reread->sampleId, created.sampleId);
    EXPECT_EQ(reread->shortfallQuantity, created.shortfallQuantity);
    EXPECT_EQ(reread->actualProducedQuantity, created.actualProducedQuantity);
    EXPECT_EQ(reread->shortfallQuantity, 10);
    EXPECT_EQ(reread->actualProducedQuantity, 12);
}

TEST_F(ProductionServiceTest, EnqueueingForAnUnknownSampleIdThrowsAndDoesNotCreateAQueueEntry) {
    m_orders->Add(MakeOrder("ORD-0001", "SMP-999", "Acme", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    EXPECT_THROW(service.Enqueue("ORD-0001", "SMP-999", 10, clock), std::invalid_argument);

    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}

// ---- SettleDueEntries ----

TEST_F(ProductionServiceTest, SettleDueEntriesOnAnEmptyQueueIsANoOp) {
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    EXPECT_NO_THROW(service.SettleDueEntries(clock));

    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}

TEST_F(ProductionServiceTest, EntryNotYetDueIsLeftInTheQueueWithOrderAndStockUntouched) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 30, clock);  // completes 30*30=900 minutes from now

    clock.Advance(std::chrono::minutes(899));
    service.SettleDueEntries(clock);

    EXPECT_EQ(m_queue->FindAllInOrder().size(), 1u);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Producing);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 100);
}

TEST_F(ProductionServiceTest, EntryExactlyAtItsCompletionTimeSettles) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 30, clock);  // 900 minutes

    clock.Advance(std::chrono::minutes(900));  // exactly at completion => boundary <=
    service.SettleDueEntries(clock);

    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 130);
}

TEST_F(ProductionServiceTest, EntryPastItsCompletionTimeSettlesRemovingItAndCreditingStockAndOrderStatus) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 30, clock);  // 900 minutes

    clock.Advance(std::chrono::minutes(1000));
    service.SettleDueEntries(clock);

    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 130);
}

TEST_F(ProductionServiceTest, OnlyDueEntriesSettleWhileTheRemainingTailStaysInQueueInOriginalOrder) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 10, 1.0, 0)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing));
    m_orders->Add(MakeOrder("ORD-0002", "SMP-001", "Acme", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 10, clock);   // completes in 10*10=100 minutes
    service.Enqueue("ORD-0002", "SMP-001", 100, clock);  // chains after, much later completion

    clock.Advance(std::chrono::minutes(150));  // past ORD-0001's completion, not ORD-0002's
    service.SettleDueEntries(clock);

    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0002")->status, OrderStatus::Producing);

    std::vector<ProductionQueueEntry> remaining = m_queue->FindAllInOrder();
    ASSERT_EQ(remaining.size(), 1u);
    EXPECT_EQ(remaining[0].orderNumber, "ORD-0002");
}

TEST_F(ProductionServiceTest, MultipleSimultaneouslyDueEntriesForDifferentSamplesAllSettleWithoutCrossContamination) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 5, 1.0, 0)));
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-002", "Silicon Ingot", 5, 1.0, 0)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing));
    m_orders->Add(MakeOrder("ORD-0002", "SMP-002", "Other Co", 20, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 10, clock);
    service.Enqueue("ORD-0002", "SMP-002", 20, clock);

    clock.Advance(std::chrono::minutes(10000));
    service.SettleDueEntries(clock);

    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0002")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 10);
    EXPECT_EQ(m_samples->FindById("SMP-002")->currentStock, 20);
}

TEST_F(ProductionServiceTest, CallingSettleDueEntriesTwiceAtTheSameTimeDoesNotDoubleCreditStock) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 10, 1.0, 0)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;
    service.Enqueue("ORD-0001", "SMP-001", 10, clock);  // 100 minutes

    clock.Advance(std::chrono::minutes(200));
    service.SettleDueEntries(clock);
    service.SettleDueEntries(clock);  // same clock time again -- must be a true no-op

    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 10);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}

TEST_F(ProductionServiceTest, MultiStepFifoSettlementMatchesTheFiftyThenHundredThenHundredAcceptanceExample) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 0)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 50, OrderStatus::Producing));
    m_orders->Add(MakeOrder("ORD-0002", "SMP-001", "Other Co", 100, OrderStatus::Producing));
    ProductionService service(*m_samples, *m_orders, *m_queue);
    FakeClock clock;

    service.Enqueue("ORD-0001", "SMP-001", 50, clock);   // completes 50 minutes from now
    service.Enqueue("ORD-0002", "SMP-001", 100, clock);  // chains: completes 150 minutes from now

    // Advance past only the first entry's completion.
    clock.Advance(std::chrono::minutes(60));
    service.SettleDueEntries(clock);

    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0002")->status, OrderStatus::Producing);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 50);
    ASSERT_EQ(m_queue->FindAllInOrder().size(), 1u);
    EXPECT_EQ(m_queue->FindAllInOrder()[0].orderNumber, "ORD-0002");

    // Advance further past the second entry's completion.
    clock.Advance(std::chrono::minutes(100));
    service.SettleDueEntries(clock);

    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0002")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 150);
    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}
