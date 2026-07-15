#include <gtest/gtest.h>

#include "Services/MonitoringService.h"
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
#include <string>
#include <vector>

namespace {

// Mirrors schema/sample.schema.json exactly.
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

class MonitoringServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("MonitoringServiceTest_") + info->name());
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
        m_clock = std::make_unique<FakeClock>();
        m_service = std::make_unique<MonitoringService>(*m_orders, *m_samples, *m_queue, *m_clock);
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
    std::unique_ptr<FakeClock> m_clock;
    std::unique_ptr<MonitoringService> m_service;
};

}  // namespace

// ---- GetOrderStatusCounts ----

TEST_F(MonitoringServiceTest, GetOrderStatusCountsIsAllZeroWhenThereAreNoOrders) {
    OrderStatusCounts counts = m_service->GetOrderStatusCounts();

    EXPECT_EQ(counts.reserved, 0);
    EXPECT_EQ(counts.confirmed, 0);
    EXPECT_EQ(counts.producing, 0);
    EXPECT_EQ(counts.released, 0);
}

TEST_F(MonitoringServiceTest, GetOrderStatusCountsTalliesEachStatusExcludingRejectedEntirely) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 5, OrderStatus::Confirmed});
    m_orders->Add(Order{"ORD-0003", "SMP-001", "Acme", 5, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0004", "SMP-001", "Acme", 5, OrderStatus::Released});
    m_orders->Add(Order{"ORD-0005", "SMP-001", "Acme", 5, OrderStatus::Rejected});
    m_orders->Add(Order{"ORD-0006", "SMP-001", "Acme", 5, OrderStatus::Rejected});

    OrderStatusCounts counts = m_service->GetOrderStatusCounts();

    EXPECT_EQ(counts.reserved, 1);
    EXPECT_EQ(counts.confirmed, 1);
    EXPECT_EQ(counts.producing, 1);
    EXPECT_EQ(counts.released, 1);
    // Rejected has no field to increment -- verified indirectly: sum of the
    // four known counters must equal 4, not 6 (would be 6 if Rejected leaked
    // into some other bucket).
    EXPECT_EQ(counts.reserved + counts.confirmed + counts.producing + counts.released, 4);
}

TEST_F(MonitoringServiceTest, GetOrderStatusCountsSettlesDueEntriesBeforeCountingSoAMaturedProducingOrderCountsAsConfirmed) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 0)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_production->Enqueue("ORD-0001", "SMP-001", 10, *m_clock);  // completes in 10 minutes
    m_clock->Advance(std::chrono::minutes(20));

    OrderStatusCounts counts = m_service->GetOrderStatusCounts();

    EXPECT_EQ(counts.producing, 0);
    EXPECT_EQ(counts.confirmed, 1);

    // Idempotency: calling again at the same fake-clock time must not
    // double-apply anything or change the result.
    OrderStatusCounts countsAgain = m_service->GetOrderStatusCounts();
    EXPECT_EQ(countsAgain.confirmed, 1);
    EXPECT_EQ(countsAgain.producing, 0);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 10);
}

// ---- GetSampleStockLevels ----

TEST_F(MonitoringServiceTest, GetSampleStockLevelsLabelsZeroStockAsDepletedAndPositiveStockAsInStock) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 0)));
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-002", "Silicon Ingot", 30, 1.0, 1)));

    std::vector<SampleStockInfo> levels = m_service->GetSampleStockLevels();

    ASSERT_EQ(levels.size(), 2u);
    for (const auto& info : levels) {
        if (info.sampleId == "SMP-001") {
            EXPECT_EQ(info.currentStock, 0);
            EXPECT_EQ(info.level, StockLevel::Depleted);
        } else if (info.sampleId == "SMP-002") {
            EXPECT_EQ(info.currentStock, 1);
            EXPECT_EQ(info.level, StockLevel::InStock);
        } else {
            FAIL() << "unexpected sampleId";
        }
    }
}

TEST_F(MonitoringServiceTest, GetSampleStockLevelsReflectsStockIncreaseFromSettlementThatRanAsPartOfThisSameCall) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 0)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_production->Enqueue("ORD-0001", "SMP-001", 10, *m_clock);
    m_clock->Advance(std::chrono::minutes(20));

    std::vector<SampleStockInfo> levels = m_service->GetSampleStockLevels();

    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].currentStock, 10);
    EXPECT_EQ(levels[0].level, StockLevel::InStock);
}

TEST_F(MonitoringServiceTest, GetSampleStockLevelsUnclaimedStockExcludesQuantityAlreadyClaimedByConfirmedAndProducingOrders) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 20, OrderStatus::Confirmed});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 15, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0003", "SMP-001", "Acme", 999, OrderStatus::Reserved});   // not claimed yet
    m_orders->Add(Order{"ORD-0004", "SMP-001", "Acme", 999, OrderStatus::Released});   // already shipped, not claimed
    m_orders->Add(Order{"ORD-0005", "SMP-001", "Acme", 999, OrderStatus::Rejected});   // never claimed

    std::vector<SampleStockInfo> levels = m_service->GetSampleStockLevels();

    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].currentStock, 100);
    EXPECT_EQ(levels[0].unclaimedStock, 65);  // 100 - (20 + 15)
}

TEST_F(MonitoringServiceTest, GetSampleStockLevelsUnclaimedStockIsFlooredAtZeroWhenOverclaimed) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 10)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Confirmed});

    std::vector<SampleStockInfo> levels = m_service->GetSampleStockLevels();

    ASSERT_EQ(levels.size(), 1u);
    EXPECT_EQ(levels[0].unclaimedStock, 0);
}
