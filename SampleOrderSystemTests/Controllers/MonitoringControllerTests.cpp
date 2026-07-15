#include <gtest/gtest.h>

#include "Controllers/MonitoringController.h"
#include "Views/MonitoringView.h"
#include "Views/ProductionLineView.h"
#include "Services/MonitoringService.h"
#include "Services/ProductionLineViewService.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Global namespace throughout, matching this repo's real committed code.
// MonitoringController takes std::function fetch callbacks (per phase-8-ui's
// DETAIL-monitoring-production-ui.md design decision #2) so it can be tested
// with hand-built fixtures independent of MonitoringService/ProductionLineViewService's
// own repository/clock wiring.

}  // namespace

TEST(MonitoringControllerTests, ShowMonitoringCallsFetchCountsAndFetchStocksExactlyOnceAndRendersBoth) {
    int countsCalls = 0;
    int stocksCalls = 0;

    OrderStatusCounts counts;
    counts.reserved = 1;
    counts.confirmed = 2;

    SampleStockInfo stock;
    stock.sampleId = "SMP-001";
    stock.sampleName = "GaAs Wafer";
    stock.currentStock = 5;
    stock.level = StockLevel::InStock;

    std::ostringstream monitoringOut;
    std::ostringstream productionOut;
    MonitoringView monitoringView(monitoringOut);
    ProductionLineView productionLineView(productionOut);

    MonitoringController controller(
        [&]() { ++countsCalls; return counts; },
        [&]() { ++stocksCalls; return std::vector<SampleStockInfo>{ stock }; },
        [&]() { return ProductionLineSnapshot{}; },
        monitoringView,
        productionLineView);

    controller.ShowMonitoring();

    EXPECT_EQ(countsCalls, 1);
    EXPECT_EQ(stocksCalls, 1);
    EXPECT_NE(monitoringOut.str().find("SMP-001"), std::string::npos);
}

TEST(MonitoringControllerTests, ShowProductionLineCallsFetchProductionLineExactlyOnceAndRendersIt) {
    int fetchCalls = 0;

    ProductionQueueEntryView entry;
    entry.orderNumber = "ORD-0001";
    entry.sampleId = "SMP-001";
    entry.sampleName = "GaAs Wafer";
    entry.shortfallQuantity = 10;
    entry.actualProducedQuantity = 12;

    ProductionLineSnapshot snapshot;
    snapshot.inProduction = entry;

    std::ostringstream monitoringOut;
    std::ostringstream productionOut;
    MonitoringView monitoringView(monitoringOut);
    ProductionLineView productionLineView(productionOut);

    MonitoringController controller(
        []() { return OrderStatusCounts{}; },
        []() { return std::vector<SampleStockInfo>{}; },
        [&]() { ++fetchCalls; return snapshot; },
        monitoringView,
        productionLineView);

    controller.ShowProductionLine();

    EXPECT_EQ(fetchCalls, 1);
    EXPECT_NE(productionOut.str().find("ORD-0001"), std::string::npos);
}

TEST(MonitoringControllerTests, ShowMonitoringPropagatesAnExceptionThrownByFetchCounts) {
    std::ostringstream monitoringOut;
    std::ostringstream productionOut;
    MonitoringView monitoringView(monitoringOut);
    ProductionLineView productionLineView(productionOut);

    MonitoringController controller(
        []() -> OrderStatusCounts { throw std::runtime_error("boom"); },
        []() { return std::vector<SampleStockInfo>{}; },
        []() { return ProductionLineSnapshot{}; },
        monitoringView,
        productionLineView);

    EXPECT_THROW(controller.ShowMonitoring(), std::runtime_error);
}

TEST(MonitoringControllerTests, ShowProductionLinePropagatesAnExceptionThrownByFetchProductionLine) {
    std::ostringstream monitoringOut;
    std::ostringstream productionOut;
    MonitoringView monitoringView(monitoringOut);
    ProductionLineView productionLineView(productionOut);

    MonitoringController controller(
        []() { return OrderStatusCounts{}; },
        []() { return std::vector<SampleStockInfo>{}; },
        []() -> ProductionLineSnapshot { throw std::runtime_error("boom"); },
        monitoringView,
        productionLineView);

    EXPECT_THROW(controller.ShowProductionLine(), std::runtime_error);
}

TEST(MonitoringControllerTests, ShowMonitoringWithEmptyStockListStillRendersTheEmptyStateThroughTheView) {
    std::ostringstream monitoringOut;
    std::ostringstream productionOut;
    MonitoringView monitoringView(monitoringOut);
    ProductionLineView productionLineView(productionOut);

    MonitoringController controller(
        []() { return OrderStatusCounts{}; },
        []() { return std::vector<SampleStockInfo>{}; },
        []() { return ProductionLineSnapshot{}; },
        monitoringView,
        productionLineView);

    controller.ShowMonitoring();

    EXPECT_FALSE(monitoringOut.str().empty());
}
