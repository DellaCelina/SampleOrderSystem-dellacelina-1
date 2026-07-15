#pragma once
#include <istream>
#include <string>

#include "../Core/IClock.h"
#include "../Services/OrderService.h"
#include "../Views/OrderView.h"

class OrderController {
public:
    OrderController(OrderService& orderService, OrderView& view, std::istream& in, IClock& clock);

    void Run();

private:
    void HandleSubmitOrder();
    void HandleApproveReject();
    void HandleRelease();

    static bool TryParsePositiveInt(const std::string& text, int& out);
    static std::string Trim(const std::string& text);
    bool ReadLine(std::string& out);
    void WaitForEnter();

    OrderService& orderService_;
    OrderView& view_;
    std::istream& in_;
    IClock& clock_;
};
