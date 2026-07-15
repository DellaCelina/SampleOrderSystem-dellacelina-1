#include <gtest/gtest.h>

#include "Repositories/ProductionQueueRepository.h"
#include "Models/ProductionQueueEntry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

ProductionQueueEntry MakeEntry(const std::string& orderNumber, const std::string& sampleId, int shortfall,
                                int actualProduced, long long enqueuedAtSeconds,
                                long long expectedCompletionAtSeconds) {
    ProductionQueueEntry entry;
    entry.orderNumber = orderNumber;
    entry.sampleId = sampleId;
    entry.shortfallQuantity = shortfall;
    entry.actualProducedQuantity = actualProduced;
    entry.enqueuedAt = MakeUtcSeconds(enqueuedAtSeconds);
    entry.expectedCompletionAt = MakeUtcSeconds(expectedCompletionAtSeconds);
    return entry;
}

class ProductionQueueRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("ProductionQueueRepositoryTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        m_schemaPath = m_testDir / "production_queue.schema.json";
        std::ofstream schemaOut(m_schemaPath, std::ios::trunc);
        schemaOut << kProductionQueueSchemaJson;
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    std::filesystem::path DataPath(const std::string& fileName = "production_queue.json") const {
        return m_testDir / fileName;
    }

    std::filesystem::path SchemaPath() const { return m_schemaPath; }

    std::filesystem::path m_testDir;
    std::filesystem::path m_schemaPath;
};

}  // namespace

TEST_F(ProductionQueueRepositoryTest, ConstructingAgainstANonExistentFileYieldsAnEmptyQueue) {
    ProductionQueueRepository repo(DataPath(), SchemaPath());

    EXPECT_TRUE(repo.FindAllInOrder().empty());
    EXPECT_EQ(repo.PeekHead(), std::nullopt);
}

TEST_F(ProductionQueueRepositoryTest, EnqueueingThreeEntriesPreservesFifoOrderInFindAllInOrderAndPeekHead) {
    ProductionQueueRepository repo(DataPath(), SchemaPath());

    ProductionQueueEntry a = MakeEntry("ORD-0001", "SMP-001", 10, 12, 1000, 2000);
    ProductionQueueEntry b = MakeEntry("ORD-0002", "SMP-001", 5, 6, 1500, 2500);
    ProductionQueueEntry c = MakeEntry("ORD-0003", "SMP-002", 8, 9, 1800, 2800);

    repo.Enqueue(a);
    repo.Enqueue(b);
    repo.Enqueue(c);

    std::vector<ProductionQueueEntry> all = repo.FindAllInOrder();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].orderNumber, "ORD-0001");
    EXPECT_EQ(all[1].orderNumber, "ORD-0002");
    EXPECT_EQ(all[2].orderNumber, "ORD-0003");

    std::optional<ProductionQueueEntry> head = repo.PeekHead();
    ASSERT_TRUE(head.has_value());
    EXPECT_EQ(head->orderNumber, "ORD-0001");
}

TEST_F(ProductionQueueRepositoryTest, RemovingTheHeadEntryLeavesTheRemainderInOrderAndAdvancesPeekHead) {
    ProductionQueueRepository repo(DataPath(), SchemaPath());
    repo.Enqueue(MakeEntry("ORD-0001", "SMP-001", 10, 12, 1000, 2000));
    repo.Enqueue(MakeEntry("ORD-0002", "SMP-001", 5, 6, 1500, 2500));
    repo.Enqueue(MakeEntry("ORD-0003", "SMP-002", 8, 9, 1800, 2800));

    repo.Remove("ORD-0001");

    std::vector<ProductionQueueEntry> remaining = repo.FindAllInOrder();
    ASSERT_EQ(remaining.size(), 2u);
    EXPECT_EQ(remaining[0].orderNumber, "ORD-0002");
    EXPECT_EQ(remaining[1].orderNumber, "ORD-0003");

    std::optional<ProductionQueueEntry> head = repo.PeekHead();
    ASSERT_TRUE(head.has_value());
    EXPECT_EQ(head->orderNumber, "ORD-0002");
}

TEST_F(ProductionQueueRepositoryTest, RemovingAMiddleEntryPreservesTheRelativeOrderOfTheRemainingEntries) {
    ProductionQueueRepository repo(DataPath(), SchemaPath());
    repo.Enqueue(MakeEntry("ORD-0001", "SMP-001", 10, 12, 1000, 2000));
    repo.Enqueue(MakeEntry("ORD-0002", "SMP-001", 5, 6, 1500, 2500));
    repo.Enqueue(MakeEntry("ORD-0003", "SMP-002", 8, 9, 1800, 2800));

    repo.Remove("ORD-0002");

    std::vector<ProductionQueueEntry> remaining = repo.FindAllInOrder();
    ASSERT_EQ(remaining.size(), 2u);
    EXPECT_EQ(remaining[0].orderNumber, "ORD-0001");
    EXPECT_EQ(remaining[1].orderNumber, "ORD-0003");
}

TEST_F(ProductionQueueRepositoryTest, RemovingAnUnknownOrderNumberThrowsAndLeavesTheQueueUnchanged) {
    ProductionQueueRepository repo(DataPath(), SchemaPath());
    repo.Enqueue(MakeEntry("ORD-0001", "SMP-001", 10, 12, 1000, 2000));

    EXPECT_THROW(repo.Remove("ORD-9999"), std::invalid_argument);

    EXPECT_EQ(repo.FindAllInOrder().size(), 1u);
}

TEST_F(ProductionQueueRepositoryTest, PersistenceRoundTripAcrossTwoIndependentInstancesPreservesTimestampsAndOrder) {
    ProductionQueueEntry first = MakeEntry("ORD-0001", "SMP-001", 10, 12, 1704067200, 1704070800);
    ProductionQueueEntry second = MakeEntry("ORD-0002", "SMP-002", 4, 5, 1704074400, 1704078000);

    {
        ProductionQueueRepository writer(DataPath(), SchemaPath());
        writer.Enqueue(first);
        writer.Enqueue(second);
    }

    ProductionQueueRepository reader(DataPath(), SchemaPath());
    std::vector<ProductionQueueEntry> all = reader.FindAllInOrder();

    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].orderNumber, first.orderNumber);
    EXPECT_EQ(all[0].sampleId, first.sampleId);
    EXPECT_EQ(all[0].shortfallQuantity, first.shortfallQuantity);
    EXPECT_EQ(all[0].actualProducedQuantity, first.actualProducedQuantity);
    EXPECT_EQ(all[0].enqueuedAt, first.enqueuedAt);
    EXPECT_EQ(all[0].expectedCompletionAt, first.expectedCompletionAt);

    EXPECT_EQ(all[1].orderNumber, second.orderNumber);
    EXPECT_EQ(all[1].enqueuedAt, second.enqueuedAt);
    EXPECT_EQ(all[1].expectedCompletionAt, second.expectedCompletionAt);
}
