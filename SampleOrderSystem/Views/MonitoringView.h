#pragma once
#include <iostream>
#include <vector>

#include "../Services/MonitoringService.h"

class MonitoringView {
public:
    explicit MonitoringView(std::ostream& out = std::cout);

    void RenderStatusCounts(const OrderStatusCounts& counts);
    void RenderSampleStocks(const std::vector<SampleStockInfo>& stocks);

private:
    std::ostream& out_;
};
