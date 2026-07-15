#include <gtest/gtest.h>

#include "Services/DummyDataGenerator.h"
#include "Services/ProductionService.h"

#include "Repositories/SampleRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "Models/ProductionQueueEntry.h"
#include "FakeClock.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
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

class DummyDataGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("DummyDataGeneratorTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        WriteSchema("sample.schema.json", kSampleSchemaJson);
        WriteSchema("order.schema.json", kOrderSchemaJson);
        WriteSchema("production_queue.schema.json", kProductionQueueSchemaJson);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    void WriteSchema(const std::string& fileName, const char* contents) const {
        std::ofstream out(m_testDir / fileName, std::ios::trunc);
        out << contents;
    }

    // Builds a fresh set of repositories rooted at a given subdirectory, so
    // two independent "runs" (e.g. for a determinism comparison) don't share
    // any file state.
    struct Repos {
        std::unique_ptr<SampleRepository> samples;
        std::unique_ptr<OrderRepository> orders;
        std::unique_ptr<ProductionQueueRepository> queue;
    };

    Repos MakeRepos(const std::string& subdir) {
        std::filesystem::path dir = m_testDir / subdir;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        Repos repos;
        repos.samples = std::make_unique<SampleRepository>(dir / "samples.json", m_testDir / "sample.schema.json");
        repos.orders = std::make_unique<OrderRepository>(dir / "orders.json", m_testDir / "order.schema.json");
        repos.queue = std::make_unique<ProductionQueueRepository>(dir / "production_queue.json",
                                                                    m_testDir / "production_queue.schema.json");
        return repos;
    }

    std::filesystem::path m_testDir;
};

}  // namespace

TEST_F(DummyDataGeneratorTest, GenerateSamplesWithZeroOrNegativeCountIsANoOp) {
    Repos repos = MakeRepos("run1");
    FakeClock clock;
    DummyDataGenerator generator(*repos.samples, *repos.orders, *repos.queue, clock, /*seed=*/1);

    EXPECT_TRUE(generator.GenerateSamples(0).empty());
    EXPECT_TRUE(generator.GenerateSamples(-1).empty());
    EXPECT_TRUE(repos.samples->FindAll().empty());
}

TEST_F(DummyDataGeneratorTest, GenerateOrdersWithEmptySampleIdsAndPositiveCountThrows) {
    Repos repos = MakeRepos("run1");
    FakeClock clock;
    DummyDataGenerator generator(*repos.samples, *repos.orders, *repos.queue, clock, /*seed=*/1);

    EXPECT_THROW(generator.GenerateOrders(3, {}), std::invalid_argument);
    EXPECT_TRUE(repos.orders->FindAll().empty());
}

TEST_F(DummyDataGeneratorTest, GenerateAllWithAFixedSeedProducesSchemaValidRecordsCoveringAllFiveStatusesDeterministically) {
    const unsigned int kSeed = 42;
    const DummyDataOptions options{/*sampleCount=*/10, /*orderCount=*/25};

    // ---- Run 1 ----
    Repos run1 = MakeRepos("run1");
    FakeClock clock1;
    DummyDataGenerator generator1(*run1.samples, *run1.orders, *run1.queue, clock1, kSeed);
    generator1.GenerateAll(options);

    std::vector<Sample> samples1 = run1.samples->FindAll();
    std::vector<Order> orders1 = run1.orders->FindAll();
    std::vector<ProductionQueueEntry> queue1 = run1.queue->FindAllInOrder();

    ASSERT_EQ(samples1.size(), 10u);
    ASSERT_EQ(orders1.size(), 25u);

    // Every sample field respects schema bounds.
    std::set<std::string> sampleIds;
    for (const Sample& s : samples1) {
        EXPECT_TRUE(sampleIds.insert(s.sampleId).second) << "duplicate sampleId " << s.sampleId;
        EXPECT_GT(s.averageProductionTimeMinutes, 0);
        EXPECT_GT(s.yield, 0.0);
        EXPECT_LE(s.yield, 1.0);
        EXPECT_GE(s.currentStock, 0);
    }

    // Every order field respects schema bounds, and all five statuses appear
    // (guaranteed at orderCount=25 >= 5).
    std::set<std::string> orderNumbers;
    std::set<OrderStatus> statusesSeen;
    for (const Order& o : orders1) {
        EXPECT_TRUE(orderNumbers.insert(o.orderNumber).second) << "duplicate orderNumber " << o.orderNumber;
        EXPECT_GT(o.quantity, 0);
        EXPECT_TRUE(sampleIds.count(o.sampleId) > 0) << "order references unknown sampleId " << o.sampleId;
        statusesSeen.insert(o.status);
    }
    EXPECT_TRUE(statusesSeen.count(OrderStatus::Reserved) > 0);
    EXPECT_TRUE(statusesSeen.count(OrderStatus::Confirmed) > 0);
    EXPECT_TRUE(statusesSeen.count(OrderStatus::Producing) > 0);
    EXPECT_TRUE(statusesSeen.count(OrderStatus::Released) > 0);
    EXPECT_TRUE(statusesSeen.count(OrderStatus::Rejected) > 0);

    // Every Producing order has a matching queue entry whose numbers were
    // derived via ProductionService's own pure math (not reimplemented).
    for (const Order& o : orders1) {
        if (o.status != OrderStatus::Producing) continue;
        auto entryIt = std::find_if(queue1.begin(), queue1.end(),
                                     [&](const ProductionQueueEntry& e) { return e.orderNumber == o.orderNumber; });
        ASSERT_NE(entryIt, queue1.end()) << "no queue entry for Producing order " << o.orderNumber;
        EXPECT_GT(entryIt->shortfallQuantity, 0);
        auto sampleIt = std::find_if(samples1.begin(), samples1.end(),
                                      [&](const Sample& s) { return s.sampleId == o.sampleId; });
        ASSERT_NE(sampleIt, samples1.end());
        EXPECT_EQ(entryIt->actualProducedQuantity,
                  ProductionService::ComputeActualQuantity(entryIt->shortfallQuantity, sampleIt->yield));
    }

    // ---- Run 2: identical seed against a fresh, identically-initialized
    // repository set must produce byte-identical generated records. ----
    Repos run2 = MakeRepos("run2");
    FakeClock clock2;
    DummyDataGenerator generator2(*run2.samples, *run2.orders, *run2.queue, clock2, kSeed);
    generator2.GenerateAll(options);

    std::vector<Sample> samples2 = run2.samples->FindAll();
    std::vector<Order> orders2 = run2.orders->FindAll();
    std::vector<ProductionQueueEntry> queue2 = run2.queue->FindAllInOrder();

    ASSERT_EQ(samples1.size(), samples2.size());
    for (size_t i = 0; i < samples1.size(); ++i) {
        EXPECT_EQ(samples1[i].sampleId, samples2[i].sampleId);
        EXPECT_EQ(samples1[i].name, samples2[i].name);
        EXPECT_EQ(samples1[i].averageProductionTimeMinutes, samples2[i].averageProductionTimeMinutes);
        EXPECT_DOUBLE_EQ(samples1[i].yield, samples2[i].yield);
        EXPECT_EQ(samples1[i].currentStock, samples2[i].currentStock);
    }

    ASSERT_EQ(orders1.size(), orders2.size());
    for (size_t i = 0; i < orders1.size(); ++i) {
        EXPECT_EQ(orders1[i].orderNumber, orders2[i].orderNumber);
        EXPECT_EQ(orders1[i].sampleId, orders2[i].sampleId);
        EXPECT_EQ(orders1[i].quantity, orders2[i].quantity);
        EXPECT_EQ(static_cast<int>(orders1[i].status), static_cast<int>(orders2[i].status));
    }

    ASSERT_EQ(queue1.size(), queue2.size());
    for (size_t i = 0; i < queue1.size(); ++i) {
        EXPECT_EQ(queue1[i].orderNumber, queue2[i].orderNumber);
        EXPECT_EQ(queue1[i].shortfallQuantity, queue2[i].shortfallQuantity);
        EXPECT_EQ(queue1[i].actualProducedQuantity, queue2[i].actualProducedQuantity);
        EXPECT_EQ(queue1[i].expectedCompletionAt, queue2[i].expectedCompletionAt);
    }
}
