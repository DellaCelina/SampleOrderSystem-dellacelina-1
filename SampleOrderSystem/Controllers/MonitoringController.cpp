#include "MonitoringController.h"

MonitoringController::MonitoringController(std::function<OrderStatusCounts()> fetchCounts,
                                             std::function<std::vector<SampleStockInfo>()> fetchStocks,
                                             std::function<ProductionLineSnapshot()> fetchProductionLine,
                                             MonitoringView& monitoringView,
                                             ProductionLineView& productionLineView)
    : fetchCounts_(std::move(fetchCounts)),
      fetchStocks_(std::move(fetchStocks)),
      fetchProductionLine_(std::move(fetchProductionLine)),
      monitoringView_(monitoringView),
      productionLineView_(productionLineView) {}

void MonitoringController::ShowMonitoring() {
    OrderStatusCounts counts = fetchCounts_();
    std::vector<SampleStockInfo> stocks = fetchStocks_();
    monitoringView_.RenderStatusCounts(counts);
    monitoringView_.RenderSampleStocks(stocks);
}

void MonitoringController::ShowProductionLine() {
    ProductionLineSnapshot snapshot = fetchProductionLine_();
    productionLineView_.RenderSnapshot(snapshot);
}
