#pragma once
#include <iostream>
#include <vector>

#include "../Models/Order.h"
#include "../Models/ProductionQueueEntry.h"
#include "../Models/Sample.h"

class DataMonitorView {
public:
    explicit DataMonitorView(std::ostream& out = std::cout);

    void Render(const std::vector<Sample>& samples, const std::vector<Order>& orders,
                const std::vector<ProductionQueueEntry>& queueEntries);

private:
    std::ostream& out_;
};
