#include <gtest/gtest.h>

#include "Controllers/DummyDataController.h"
#include "Services/DummyDataGenerator.h"

#include "Repositories/SampleRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "FakeClock.h"

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

class DummyDataControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("DummyDataControllerTest_") + info->name());
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
        m_clock = std::make_unique<FakeClock>();
        m_generator = std::make_unique<DummyDataGenerator>(*m_samples, *m_orders, *m_queue, *m_clock, /*seed=*/12345u);
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
    std::unique_ptr<FakeClock> m_clock;
    std::unique_ptr<DummyDataGenerator> m_generator;
};

}  // namespace

TEST_F(DummyDataControllerTest, RunWithExplicitCountsPersistsThatManySamplesAndOrders) {
    std::ostringstream out;
    DummyDataController controller(*m_generator, out);

    controller.Run(5, 8);

    EXPECT_EQ(m_samples->FindAll().size(), 5u);
    EXPECT_EQ(m_orders->FindAll().size(), 8u);
}

TEST_F(DummyDataControllerTest, RunWithDefaultArgumentsSucceedsAndGeneratesNonZeroRecords) {
    std::ostringstream out;
    DummyDataController controller(*m_generator, out);

    controller.Run();

    EXPECT_FALSE(m_samples->FindAll().empty());
    EXPECT_FALSE(m_orders->FindAll().empty());
}

TEST_F(DummyDataControllerTest, RunWritesTheActuallyGeneratedCountsToTheInjectedOstream) {
    std::ostringstream out;
    DummyDataController controller(*m_generator, out);

    controller.Run(3, 4);

    const std::string text = out.str();
    EXPECT_NE(text.find("3"), std::string::npos);
    EXPECT_NE(text.find("4"), std::string::npos);
    EXPECT_FALSE(text.empty());
}

TEST_F(DummyDataControllerTest, CallingRunTwiceAccumulatesRatherThanCorruptingPriorData) {
    std::ostringstream out;
    DummyDataController controller(*m_generator, out);

    controller.Run(3, 4);
    ASSERT_EQ(m_samples->FindAll().size(), 3u);
    ASSERT_EQ(m_orders->FindAll().size(), 4u);

    controller.Run(2, 3);

    EXPECT_EQ(m_samples->FindAll().size(), 5u);
    EXPECT_EQ(m_orders->FindAll().size(), 7u);
}

TEST_F(DummyDataControllerTest, RunWithZeroCountsGeneratesNoNewRecordsAndDoesNotThrow) {
    std::ostringstream out;
    DummyDataController controller(*m_generator, out);

    EXPECT_NO_THROW(controller.Run(0, 0));

    EXPECT_TRUE(m_samples->FindAll().empty());
    EXPECT_TRUE(m_orders->FindAll().empty());
}
