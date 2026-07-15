#include <gtest/gtest.h>

#include "Views/DataMonitorView.h"
#include "Models/Sample.h"
#include "Models/Order.h"
#include "Models/ProductionQueueEntry.h"

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace throughout, matching this repo's real committed code.

Sample MakeSample(const std::string& id, const std::string& name, int stock) {
    Sample s;
    s.sampleId = id;
    s.name = name;
    s.averageProductionTimeMinutes = 30;
    s.yield = 0.9;
    s.currentStock = stock;
    return s;
}

Order MakeOrder(const std::string& orderNumber, const std::string& sampleId, int quantity, OrderStatus status) {
    Order o;
    o.orderNumber = orderNumber;
    o.sampleId = sampleId;
    o.customerName = "Acme Corp";
    o.quantity = quantity;
    o.status = status;
    return o;
}

ProductionQueueEntry MakeEntry(const std::string& orderNumber, const std::string& sampleId, int shortfall, int actual) {
    ProductionQueueEntry e;
    e.orderNumber = orderNumber;
    e.sampleId = sampleId;
    e.shortfallQuantity = shortfall;
    e.actualProducedQuantity = actual;
    e.enqueuedAt = std::chrono::system_clock::time_point{ std::chrono::seconds{1704067200} };
    e.expectedCompletionAt = std::chrono::system_clock::time_point{ std::chrono::seconds{1704067800} };
    return e;
}

}  // namespace

TEST(DataMonitorViewTests, RenderShowsOneLinePerSampleOrderAndQueueEntry) {
    std::ostringstream out;
    DataMonitorView view(out);

    std::vector<Sample> samples = { MakeSample("SMP-001", "GaAs Wafer", 42) };
    std::vector<Order> orders = { MakeOrder("ORD-0001", "SMP-001", 10, OrderStatus::Reserved) };
    std::vector<ProductionQueueEntry> queue = { MakeEntry("ORD-0002", "SMP-001", 5, 6) };

    view.Render(samples, orders, queue);

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("GaAs Wafer"), std::string::npos);
    EXPECT_NE(text.find("42"), std::string::npos);
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
}

TEST(DataMonitorViewTests, RenderWithAllThreeVectorsEmptyProducesExplicitNoRecordsMessages) {
    std::ostringstream out;
    DataMonitorView view(out);

    view.Render({}, {}, {});

    EXPECT_FALSE(out.str().empty());
}

TEST(DataMonitorViewTests, RenderWithOnlyOrdersEmptyStillRendersSamplesAndQueue) {
    std::ostringstream out;
    DataMonitorView view(out);

    std::vector<Sample> samples = { MakeSample("SMP-001", "GaAs Wafer", 42) };
    std::vector<ProductionQueueEntry> queue = { MakeEntry("ORD-0002", "SMP-001", 5, 6) };

    view.Render(samples, {}, queue);

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
}

TEST(DataMonitorViewTests, RenderDoesNotReorderTheQueueEntriesVector) {
    std::ostringstream out;
    DataMonitorView view(out);

    std::vector<ProductionQueueEntry> queue = {
        MakeEntry("ORD-0003", "SMP-003", 1, 1),
        MakeEntry("ORD-0002", "SMP-002", 5, 6),
    };

    view.Render({}, {}, queue);

    const std::string text = out.str();
    const size_t posThird = text.find("ORD-0003");
    const size_t posSecond = text.find("ORD-0002");
    ASSERT_NE(posThird, std::string::npos);
    ASSERT_NE(posSecond, std::string::npos);
    EXPECT_LT(posThird, posSecond);
}

TEST(DataMonitorViewTests, RenderIsUnfilteredAndShowsRejectedAndReleasedOrders) {
    std::ostringstream out;
    DataMonitorView view(out);

    std::vector<Order> orders = {
        MakeOrder("ORD-0004", "SMP-001", 10, OrderStatus::Rejected),
        MakeOrder("ORD-0005", "SMP-001", 20, OrderStatus::Released),
    };

    view.Render({}, orders, {});

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0004"), std::string::npos);
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Rejected)), std::string::npos);
    EXPECT_NE(text.find("ORD-0005"), std::string::npos);
    EXPECT_NE(text.find(OrderStatusToString(OrderStatus::Released)), std::string::npos);
}
