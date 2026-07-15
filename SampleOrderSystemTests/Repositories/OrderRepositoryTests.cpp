#include <gtest/gtest.h>

#include "Repositories/OrderRepository.h"
#include "Models/Order.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

Order MakeOrder(const std::string& orderNumber, const std::string& sampleId, const std::string& customerName,
                 int quantity, OrderStatus status) {
    Order order;
    order.orderNumber = orderNumber;
    order.sampleId = sampleId;
    order.customerName = customerName;
    order.quantity = quantity;
    order.status = status;
    return order;
}

class OrderRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_testDir = std::filesystem::path(::testing::TempDir()) /
                    (std::string("OrderRepositoryTest_") + info->name());
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
        std::filesystem::create_directories(m_testDir, ec);

        m_schemaPath = m_testDir / "order.schema.json";
        std::ofstream schemaOut(m_schemaPath, std::ios::trunc);
        schemaOut << kOrderSchemaJson;
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_testDir, ec);
    }

    std::filesystem::path DataPath(const std::string& fileName = "orders.json") const {
        return m_testDir / fileName;
    }

    std::filesystem::path SchemaPath() const { return m_schemaPath; }

    // Writes an orders.json array directly to disk, bypassing OrderRepository,
    // to simulate pre-existing history from a prior application run.
    void SeedOrdersFile(const std::vector<Order>& orders, const std::string& fileName = "orders.json") const {
        std::string json = "[";
        for (size_t i = 0; i < orders.size(); ++i) {
            const Order& o = orders[i];
            if (i > 0) json += ",";
            json += "{\"orderNumber\":\"" + o.orderNumber + "\",\"sampleId\":\"" + o.sampleId +
                    "\",\"customerName\":\"" + o.customerName + "\",\"quantity\":" + std::to_string(o.quantity) +
                    ",\"status\":\"" + OrderStatusToString(o.status) + "\"}";
        }
        json += "]";
        std::ofstream out(DataPath(fileName), std::ios::trunc);
        out << json;
    }

    std::filesystem::path m_testDir;
    std::filesystem::path m_schemaPath;
};

}  // namespace

TEST_F(OrderRepositoryTest, ConstructingAgainstANonExistentFileMakesNextOrderNumberReturnOrd0001) {
    OrderRepository repo(DataPath(), SchemaPath());

    EXPECT_EQ(repo.NextOrderNumber(), "ORD-0001");
}

TEST_F(OrderRepositoryTest, RestartSafeSequenceDerivationContinuesFromTheMaxExistingSuffixAcrossInstances) {
    SeedOrdersFile({
        MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved),
        MakeOrder("ORD-0002", "SMP-001", "Acme", 5, OrderStatus::Reserved),
        MakeOrder("ORD-0003", "SMP-001", "Acme", 5, OrderStatus::Reserved),
    });

    OrderRepository repo(DataPath(), SchemaPath());

    EXPECT_EQ(repo.NextOrderNumber(), "ORD-0004");
}

TEST_F(OrderRepositoryTest, SequenceDerivationTakesTheMaxSuffixNotTheRecordCountWhenThereAreGapsInTheHistory) {
    SeedOrdersFile({
        MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved),
        MakeOrder("ORD-0005", "SMP-001", "Acme", 5, OrderStatus::Reserved),
    });

    OrderRepository repo(DataPath(), SchemaPath());

    // max(1, 5) + 1 = 6, not count(2) + 1 = 3.
    EXPECT_EQ(repo.NextOrderNumber(), "ORD-0006");
}

TEST_F(OrderRepositoryTest, TwoNextOrderNumberCallsOnTheSameInstanceWithoutAnInterveningAddStillAdvance) {
    OrderRepository repo(DataPath(), SchemaPath());

    EXPECT_EQ(repo.NextOrderNumber(), "ORD-0001");
    EXPECT_EQ(repo.NextOrderNumber(), "ORD-0002");
}

TEST_F(OrderRepositoryTest, AddWithADuplicateOrderNumberThrowsAndLeavesTheTableUnchanged) {
    OrderRepository repo(DataPath(), SchemaPath());
    Order order = MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved);
    repo.Add(order);

    Order duplicate = MakeOrder("ORD-0001", "SMP-002", "Other Co", 3, OrderStatus::Reserved);
    EXPECT_THROW(repo.Add(duplicate), std::invalid_argument);
    EXPECT_EQ(repo.FindAll().size(), 1u);
}

TEST_F(OrderRepositoryTest, FindBySampleIdReturnsOnlyOrdersMatchingThatSampleAcrossAllStatuses) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved));
    repo.Add(MakeOrder("ORD-0002", "SMP-001", "Acme", 5, OrderStatus::Rejected));
    repo.Add(MakeOrder("ORD-0003", "SMP-002", "Other Co", 5, OrderStatus::Confirmed));

    std::vector<Order> matches = repo.FindBySampleId("SMP-001");

    ASSERT_EQ(matches.size(), 2u);
    for (const Order& o : matches) {
        EXPECT_EQ(o.sampleId, "SMP-001");
    }
}

TEST_F(OrderRepositoryTest, FindBySampleIdReturnsEmptyForASampleIdWithNoOrdersWithoutThrowing) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved));

    EXPECT_TRUE(repo.FindBySampleId("SMP-999").empty());
}

TEST_F(OrderRepositoryTest, FindByOrderNumberReturnsNulloptForAnUnknownOrderNumber) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved));

    EXPECT_EQ(repo.FindByOrderNumber("ORD-9999"), std::nullopt);
}

TEST_F(OrderRepositoryTest, FindByStatusReturnsExactlyTheOrdersMatchingEachOfTheFiveStatusesNoMore) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "A", 1, OrderStatus::Reserved));
    repo.Add(MakeOrder("ORD-0002", "SMP-001", "B", 1, OrderStatus::Confirmed));
    repo.Add(MakeOrder("ORD-0003", "SMP-001", "C", 1, OrderStatus::Producing));
    repo.Add(MakeOrder("ORD-0004", "SMP-001", "D", 1, OrderStatus::Released));
    repo.Add(MakeOrder("ORD-0005", "SMP-001", "E", 1, OrderStatus::Rejected));

    auto expectExactlyOne = [&](OrderStatus status, const std::string& expectedOrderNumber) {
        std::vector<Order> matches = repo.FindByStatus(status);
        ASSERT_EQ(matches.size(), 1u);
        EXPECT_EQ(matches[0].orderNumber, expectedOrderNumber);
    };

    expectExactlyOne(OrderStatus::Reserved, "ORD-0001");
    expectExactlyOne(OrderStatus::Confirmed, "ORD-0002");
    expectExactlyOne(OrderStatus::Producing, "ORD-0003");
    expectExactlyOne(OrderStatus::Released, "ORD-0004");
    expectExactlyOne(OrderStatus::Rejected, "ORD-0005");
}

TEST_F(OrderRepositoryTest, UpdateStatusChangesOnlyTheTargetOrdersStatusFieldLeavingEverythingElseUntouched) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved));
    repo.Add(MakeOrder("ORD-0002", "SMP-002", "Other Co", 3, OrderStatus::Reserved));

    repo.UpdateStatus("ORD-0001", OrderStatus::Confirmed);

    std::optional<Order> updated = repo.FindByOrderNumber("ORD-0001");
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->status, OrderStatus::Confirmed);
    EXPECT_EQ(updated->sampleId, "SMP-001");
    EXPECT_EQ(updated->customerName, "Acme");
    EXPECT_EQ(updated->quantity, 5);

    std::optional<Order> untouched = repo.FindByOrderNumber("ORD-0002");
    ASSERT_TRUE(untouched.has_value());
    EXPECT_EQ(untouched->status, OrderStatus::Reserved);
}

TEST_F(OrderRepositoryTest, UpdateStatusOnAnUnknownOrderNumberThrowsAndLeavesTheTableUnchanged) {
    OrderRepository repo(DataPath(), SchemaPath());
    repo.Add(MakeOrder("ORD-0001", "SMP-001", "Acme", 5, OrderStatus::Reserved));

    EXPECT_THROW(repo.UpdateStatus("ORD-9999", OrderStatus::Confirmed), std::invalid_argument);

    std::optional<Order> unchanged = repo.FindByOrderNumber("ORD-0001");
    ASSERT_TRUE(unchanged.has_value());
    EXPECT_EQ(unchanged->status, OrderStatus::Reserved);
}

TEST_F(OrderRepositoryTest, PersistenceRoundTripAcrossTwoIndependentInstancesIncludingSequenceContinuation) {
    {
        OrderRepository writer(DataPath(), SchemaPath());
        writer.Add(MakeOrder(writer.NextOrderNumber(), "SMP-001", "Acme", 5, OrderStatus::Reserved));
        writer.Add(MakeOrder(writer.NextOrderNumber(), "SMP-002", "Other Co", 3, OrderStatus::Reserved));
        writer.UpdateStatus("ORD-0001", OrderStatus::Confirmed);
    }

    OrderRepository reader(DataPath(), SchemaPath());
    std::vector<Order> all = reader.FindAll();

    ASSERT_EQ(all.size(), 2u);
    std::optional<Order> first = reader.FindByOrderNumber("ORD-0001");
    std::optional<Order> second = reader.FindByOrderNumber("ORD-0002");
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->status, OrderStatus::Confirmed);
    EXPECT_EQ(second->status, OrderStatus::Reserved);
    EXPECT_EQ(reader.NextOrderNumber(), "ORD-0003");
}
