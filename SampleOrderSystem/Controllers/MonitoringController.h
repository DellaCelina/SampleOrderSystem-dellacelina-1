#pragma once
#include <functional>
#include <vector>

#include "../Services/MonitoringService.h"
#include "../Services/ProductionLineViewService.h"
#include "../Views/MonitoringView.h"
#include "../Views/ProductionLineView.h"

class MonitoringController {
public:
    MonitoringController(std::function<OrderStatusCounts()> fetchCounts,
                          std::function<std::vector<SampleStockInfo>()> fetchStocks,
                          std::function<ProductionLineSnapshot()> fetchProductionLine,
                          MonitoringView& monitoringView,
                          ProductionLineView& productionLineView);

    void ShowMonitoring();
    void ShowProductionLine();

private:
    std::function<OrderStatusCounts()> fetchCounts_;
    std::function<std::vector<SampleStockInfo>()> fetchStocks_;
    std::function<ProductionLineSnapshot()> fetchProductionLine_;
    MonitoringView& monitoringView_;
    ProductionLineView& productionLineView_;
};
