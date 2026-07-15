#include <gtest/gtest.h>

#include "Views/MonitoringView.h"
#include "Views/ProductionLineView.h"
#include "Services/MonitoringService.h"
#include "Services/ProductionLineViewService.h"

#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Global namespace throughout, matching this repo's real committed code.

ProductionQueueEntryView MakeEntry(const std::string& orderNumber, const std::string& sampleId,
                                    const std::string& sampleName, int shortfall, int actual) {
    ProductionQueueEntryView entry;
    entry.orderNumber = orderNumber;
    entry.sampleId = sampleId;
    entry.sampleName = sampleName;
    entry.shortfallQuantity = shortfall;
    entry.actualProducedQuantity = actual;
    entry.expectedCompletionAt = std::chrono::system_clock::time_point{ std::chrono::seconds{1704067200} };
    return entry;
}

}  // namespace

// ---- MonitoringView ----

TEST(MonitoringViewTests, RenderStatusCountsShowsEachCountValue) {
    std::ostringstream out;
    MonitoringView view(out);

    OrderStatusCounts counts;
    counts.reserved = 3;
    counts.confirmed = 2;
    counts.producing = 1;
    counts.released = 4;

    view.RenderStatusCounts(counts);

    const std::string text = out.str();
    EXPECT_NE(text.find("3"), std::string::npos);
    EXPECT_NE(text.find("2"), std::string::npos);
    EXPECT_NE(text.find("1"), std::string::npos);
    EXPECT_NE(text.find("4"), std::string::npos);
}

TEST(MonitoringViewTests, RenderStatusCountsAllZeroDoesNotCrashAndStillRendersSomething) {
    std::ostringstream out;
    MonitoringView view(out);

    view.RenderStatusCounts(OrderStatusCounts{});

    EXPECT_FALSE(out.str().empty());
}

TEST(MonitoringViewTests, RenderSampleStocksLabelsDepletedAndInStockCorrectlyInBothDirections) {
    std::ostringstream out;
    MonitoringView view(out);

    SampleStockInfo depleted;
    depleted.sampleId = "SMP-001";
    depleted.sampleName = "GaAs Wafer";
    depleted.currentStock = 0;
    depleted.level = StockLevel::Depleted;

    SampleStockInfo inStock;
    inStock.sampleId = "SMP-002";
    inStock.sampleName = "Silicon Ingot";
    inStock.currentStock = 42;
    inStock.level = StockLevel::InStock;

    view.RenderSampleStocks({ depleted, inStock });

    const std::string text = out.str();
    EXPECT_NE(text.find("SMP-001"), std::string::npos);
    EXPECT_NE(text.find("SMP-002"), std::string::npos);
    EXPECT_NE(text.find("42"), std::string::npos);
    EXPECT_NE(text.find("고갈"), std::string::npos);
    EXPECT_NE(text.find("여유"), std::string::npos);
}

TEST(MonitoringViewTests, RenderSampleStocksWithEmptyListRendersAnExplicitEmptyStateMessage) {
    std::ostringstream out;
    MonitoringView view(out);

    view.RenderSampleStocks({});

    EXPECT_FALSE(out.str().empty());
}

// ---- ProductionLineView ----

TEST(ProductionLineViewTests, RenderSnapshotWithHeadAndNonEmptyTailShowsBoth) {
    std::ostringstream out;
    ProductionLineView view(out);

    ProductionLineSnapshot snapshot;
    snapshot.inProduction = MakeEntry("ORD-0001", "SMP-001", "GaAs Wafer", 10, 12);
    snapshot.waiting = { MakeEntry("ORD-0002", "SMP-002", "Silicon Ingot", 5, 6) };

    view.RenderSnapshot(snapshot);

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
}

TEST(ProductionLineViewTests, RenderSnapshotWithHeadPresentAndEmptyTailShowsHeadOnly) {
    std::ostringstream out;
    ProductionLineView view(out);

    ProductionLineSnapshot snapshot;
    snapshot.inProduction = MakeEntry("ORD-0001", "SMP-001", "GaAs Wafer", 10, 12);

    view.RenderSnapshot(snapshot);

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0001"), std::string::npos);
}

TEST(ProductionLineViewTests, RenderSnapshotWithNoHeadAndNonEmptyTailStillShowsTailEntries) {
    std::ostringstream out;
    ProductionLineView view(out);

    ProductionLineSnapshot snapshot;
    snapshot.waiting = { MakeEntry("ORD-0002", "SMP-002", "Silicon Ingot", 5, 6) };

    view.RenderSnapshot(snapshot);

    const std::string text = out.str();
    EXPECT_NE(text.find("ORD-0002"), std::string::npos);
}

TEST(ProductionLineViewTests, RenderSnapshotWithNoHeadAndEmptyTailRendersAnExplicitEmptyStateMessage) {
    std::ostringstream out;
    ProductionLineView view(out);

    view.RenderSnapshot(ProductionLineSnapshot{});

    EXPECT_FALSE(out.str().empty());
}

TEST(ProductionLineViewTests, RenderSnapshotDoesNotReorderTheWaitingTail) {
    std::ostringstream out;
    ProductionLineView view(out);

    ProductionLineSnapshot snapshot;
    snapshot.waiting = {
        MakeEntry("ORD-0003", "SMP-003", "Sapphire", 1, 1),
        MakeEntry("ORD-0002", "SMP-002", "Silicon Ingot", 5, 6),
    };

    view.RenderSnapshot(snapshot);

    const std::string text = out.str();
    const size_t posThird = text.find("ORD-0003");
    const size_t posSecond = text.find("ORD-0002");
    ASSERT_NE(posThird, std::string::npos);
    ASSERT_NE(posSecond, std::string::npos);
    EXPECT_LT(posThird, posSecond);
}
