#pragma once
#include <iostream>

#include "../Services/ProductionLineViewService.h"

class ProductionLineView {
public:
    explicit ProductionLineView(std::ostream& out = std::cout);

    void RenderSnapshot(const ProductionLineSnapshot& snapshot);

private:
    std::ostream& out_;

    void RenderEntry(const ProductionQueueEntryView& entry);
};
