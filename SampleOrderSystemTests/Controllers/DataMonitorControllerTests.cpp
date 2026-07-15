#include <gtest/gtest.h>

#include "Controllers/DataMonitorController.h"
#include "Views/DataMonitorView.h"
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
#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace throughout, matching this repo's real committed code.

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

class DataMonitorControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("DataMonitorControllerTest_") + info->name());
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
};

}  // namespace

TEST_F(DataMonitorControllerTest, RunSettlesDueEntriesFirstSoRenderedOutputReflectsPostSettlementState) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Producing));
    m_production->Enqueue("ORD-0001", "SMP-001", 30, *m_clock);  // completes 900 minutes from now
    m_clock->Advance(std::chrono::minutes(1000));  // already past completion

    std::ostringstream out;
    DataMonitorView view(out);
    DataMonitorController controller(*m_clock, *m_production, *m_samples, *m_orders, *m_queue, view);

    controller.Run();

    const std::string text = out.str();
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Confirmed)), std::string::npos);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Confirmed);
    EXPECT_EQ(m_samples->FindById("SMP-001")->currentStock, 130);
    EXPECT_TRUE(m_queue->FindAllInOrder().empty());
}

TEST_F(DataMonitorControllerTest, RunWithAnEntryNotYetDueLeavesOrderProducingAndQueueEntryPresent) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 30, OrderStatus::Producing));
    m_production->Enqueue("ORD-0001", "SMP-001", 30, *m_clock);  // 900 minutes
    m_clock->Advance(std::chrono::minutes(1));  // nowhere near due

    std::ostringstream out;
    DataMonitorView view(out);
    DataMonitorController controller(*m_clock, *m_production, *m_samples, *m_orders, *m_queue, view);

    controller.Run();

    const std::string text = out.str();
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Producing)), std::string::npos);
    EXPECT_EQ(m_orders->FindByOrderNumber("ORD-0001")->status, OrderStatus::Producing);
    EXPECT_EQ(m_queue->FindAllInOrder().size(), 1u);
}

TEST_F(DataMonitorControllerTest, RunWithAllThreeTablesEmptyProducesTheEmptyStateOutput) {
    std::ostringstream out;
    DataMonitorView view(out);
    DataMonitorController controller(*m_clock, *m_production, *m_samples, *m_orders, *m_queue, view);

    controller.Run();

    EXPECT_FALSE(out.str().empty());
}

TEST_F(DataMonitorControllerTest, RunCalledTwiceInARowWithoutClockAdvancingProducesIdenticalOutput) {
    ASSERT_TRUE(m_samples->Add(MakeSample("SMP-001", "GaAs Wafer", 30, 1.0, 100)));
    m_orders->Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 10, OrderStatus::Reserved));

    std::ostringstream firstOut;
    DataMonitorView firstView(firstOut);
    DataMonitorController firstController(*m_clock, *m_production, *m_samples, *m_orders, *m_queue, firstView);
    firstController.Run();

    std::ostringstream secondOut;
    DataMonitorView secondView(secondOut);
    DataMonitorController secondController(*m_clock, *m_production, *m_samples, *m_orders, *m_queue, secondView);
    secondController.Run();

    EXPECT_EQ(firstOut.str(), secondOut.str());
}
