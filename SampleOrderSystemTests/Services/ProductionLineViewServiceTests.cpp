#include <gtest/gtest.h>

#include "Services/ProductionLineViewService.h"
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

class ProductionLineViewServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("ProductionLineViewServiceTest_") + info->name());
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
        m_service = std::make_unique<ProductionLineViewService>(*m_queue, *m_orders, *m_samples, *m_clock);
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
    std::unique_ptr<ProductionLineViewService> m_service;
};

}  // namespace

TEST_F(ProductionLineViewServiceTest, GetSnapshotOnAnEmptyQueueReturnsNulloptInProductionAndEmptyWaiting) {
    ProductionLineSnapshot snapshot = m_service->GetSnapshot();

    EXPECT_FALSE(snapshot.inProduction.has_value());
    EXPECT_TRUE(snapshot.waiting.empty());
}

TEST_F(ProductionLineViewServiceTest, GetSnapshotWithThreeEntriesPutsTheFifoHeadInProductionAndKeepsTheRestInOrder) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 100, 1.0, 0)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0003", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_production->Enqueue("ORD-0001", "SMP-001", 1, *m_clock);
    m_production->Enqueue("ORD-0002", "SMP-001", 1, *m_clock);
    m_production->Enqueue("ORD-0003", "SMP-001", 1, *m_clock);

    ProductionLineSnapshot snapshot = m_service->GetSnapshot();

    ASSERT_TRUE(snapshot.inProduction.has_value());
    EXPECT_EQ(snapshot.inProduction->orderNumber, "ORD-0001");
    EXPECT_EQ(snapshot.inProduction->sampleName, "GaAs Wafer");
    ASSERT_EQ(snapshot.waiting.size(), 2u);
    EXPECT_EQ(snapshot.waiting[0].orderNumber, "ORD-0002");
    EXPECT_EQ(snapshot.waiting[1].orderNumber, "ORD-0003");
}

TEST_F(ProductionLineViewServiceTest, GetSnapshotSettlesDueEntriesBeforeReadingSoAMaturedHeadIsNotReportedAsInProduction) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 1, 1.0, 0)));
    m_orders->Add(Order{"ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Producing});
    m_orders->Add(Order{"ORD-0002", "SMP-001", "Acme", 20, OrderStatus::Producing});
    m_production->Enqueue("ORD-0001", "SMP-001", 10, *m_clock);   // completes in 10 minutes
    m_production->Enqueue("ORD-0002", "SMP-001", 20, *m_clock);  // chains after

    m_clock->Advance(std::chrono::minutes(15));  // past ORD-0001's completion only

    ProductionLineSnapshot snapshot = m_service->GetSnapshot();

    ASSERT_TRUE(snapshot.inProduction.has_value());
    EXPECT_EQ(snapshot.inProduction->orderNumber, "ORD-0002");
    EXPECT_TRUE(snapshot.waiting.empty());
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);

    // Idempotency: calling again at the same time must not change the result
    // or double-apply settlement.
    ProductionLineSnapshot snapshotAgain = m_service->GetSnapshot();
    ASSERT_TRUE(snapshotAgain.inProduction.has_value());
    EXPECT_EQ(snapshotAgain.inProduction->orderNumber, "ORD-0002");
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 10);
}

TEST_F(ProductionLineViewServiceTest, GetSnapshotFallsBackToRawSampleIdWhenTheReferencedSampleIsMissing) {
    // Deliberately no sample registered for "SMP-GHOST" -- simulates an
    // inconsistency this read-only display service must not crash on.
    m_orders->Add(Order{"ORD-0001", "SMP-GHOST", "Acme", 10, OrderStatus::Producing});
    ProductionQueueEntry ghostEntry;
    ghostEntry.orderNumber = "ORD-0001";
    ghostEntry.sampleId = "SMP-GHOST";
    ghostEntry.shortfallQuantity = 5;
    ghostEntry.actualProducedQuantity = 5;
    ghostEntry.enqueuedAt = m_clock->Now();
    ghostEntry.expectedCompletionAt = m_clock->Now() + std::chrono::minutes(1000);
    m_queue->Enqueue(ghostEntry);

    ProductionLineSnapshot snapshot = m_service->GetSnapshot();

    ASSERT_TRUE(snapshot.inProduction.has_value());
    EXPECT_EQ(snapshot.inProduction->sampleName, "SMP-GHOST");
}
