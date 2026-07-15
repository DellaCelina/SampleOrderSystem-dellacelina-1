#include "OrderController.h"

#include <cctype>

namespace {

bool IsAllDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

}  // namespace

OrderController::OrderController(OrderService& orderService, OrderView& view, std::istream& in, IClock& clock)
    : orderService_(orderService), view_(view), in_(in), clock_(clock) {}

std::string OrderController::Trim(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

bool OrderController::ReadLine(std::string& out) {
    std::string line;
    if (!std::getline(in_, line)) {
        return false;
    }
    out = Trim(line);
    return true;
}

// Blocks on one line of input so a just-rendered result/list message isn't
// immediately wiped by the next screen-clearing redraw (ShowOrderMenu's or
// ShowPendingApprovals' header both clear the terminal). Harmless no-op if
// the stream is already exhausted (real EOF simply fails to read again).
void OrderController::WaitForEnter() {
    view_.ShowPrompt("계속하려면 Enter 키를 누르세요...");
    std::string discarded;
    ReadLine(discarded);
}

bool OrderController::TryParsePositiveInt(const std::string& text, int& out) {
    if (!IsAllDigits(text)) {
        return false;
    }
    try {
        std::size_t pos = 0;
        const long long value = std::stoll(text, &pos);
        if (pos != text.size() || value <= 0) {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    } catch (...) {
        return false;
    }
}

void OrderController::Run() {
    for (;;) {
        view_.ShowOrderMenu();
        std::string choice;
        if (!ReadLine(choice)) {
            return;
        }

        if (choice == "1") {
            HandleSubmitOrder();
        } else if (choice == "2") {
            HandleApproveReject();
        } else if (choice == "3") {
            HandleRelease();
        } else if (choice == "4") {
            return;
        } else {
            view_.ShowInvalidMenuChoice();
        }
    }
}

void OrderController::HandleSubmitOrder() {
    std::string sampleId;
    std::string customerName;
    std::string quantityText;
    view_.ShowPrompt("시료 ID > ");
    const bool gotSampleId = ReadLine(sampleId);
    view_.ShowPrompt("고객명 > ");
    const bool gotCustomerName = gotSampleId && ReadLine(customerName);
    view_.ShowPrompt("주문 수량 > ");
    const bool gotQuantity = gotCustomerName && ReadLine(quantityText);
    if (!gotSampleId || !gotCustomerName || !gotQuantity) {
        view_.ShowSubmitOrderResult(false, "", "입력이 예기치 않게 종료되었습니다");
        return;
    }

    int quantity = 0;
    if (sampleId.empty()) {
        view_.ShowSubmitOrderResult(false, "", "시료 ID는 비어 있을 수 없습니다");
        WaitForEnter();
        return;
    }
    if (customerName.empty()) {
        view_.ShowSubmitOrderResult(false, "", "고객명은 비어 있을 수 없습니다");
        WaitForEnter();
        return;
    }
    if (!TryParsePositiveInt(quantityText, quantity)) {
        view_.ShowSubmitOrderResult(false, "", "주문 수량은 양의 정수여야 합니다");
        WaitForEnter();
        return;
    }

    OrderServiceResult<Order> result = orderService_.SubmitOrder(sampleId, customerName, quantity);
    if (result.Ok()) {
        view_.ShowSubmitOrderResult(true, result.Value().orderNumber, "");
    } else {
        view_.ShowSubmitOrderResult(false, "", result.Error().message);
    }
    WaitForEnter();
}

void OrderController::HandleApproveReject() {
    for (;;) {
        std::vector<Order> orders = orderService_.ListPendingApprovals();
        if (orders.empty()) {
            view_.ShowNoPendingApprovals();
            WaitForEnter();
            return;
        }
        view_.ShowPendingApprovals(orders);

        std::string orderNumber;
        view_.ShowPrompt("주문번호 (0=뒤로가기) > ");
        if (!ReadLine(orderNumber)) {
            return;
        }
        if (orderNumber.empty() || orderNumber == "0") {
            return;
        }

        std::string actionLine;
        view_.ShowPrompt("승인(A) / 거절(R) > ");
        if (!ReadLine(actionLine)) {
            return;
        }
        char action = actionLine.empty() ? '\0' : static_cast<char>(std::toupper(static_cast<unsigned char>(actionLine[0])));

        if (action == 'A') {
            OrderServiceResult<Order> result = orderService_.Approve(orderNumber, clock_);
            if (result.Ok()) {
                view_.ShowApproveResult(true, orderNumber, result.Value().status, "");
            } else {
                view_.ShowApproveResult(false, orderNumber, OrderStatus::Confirmed, result.Error().message);
            }
        } else if (action == 'R') {
            OrderServiceResult<Order> result = orderService_.Reject(orderNumber);
            if (result.Ok()) {
                view_.ShowRejectResult(true, orderNumber, "");
            } else {
                view_.ShowRejectResult(false, orderNumber, result.Error().message);
            }
        } else {
            view_.ShowInvalidApprovalAction();
        }
        WaitForEnter();
    }
}

void OrderController::HandleRelease() {
    std::vector<Order> releasable = orderService_.ListReleasable();
    if (releasable.empty()) {
        view_.ShowNoReleasableOrders();
        WaitForEnter();
        return;
    }
    view_.ShowReleasableOrders(releasable);

    std::string orderNumber;
    view_.ShowPrompt("주문번호 > ");
    if (!ReadLine(orderNumber)) {
        return;
    }
    if (orderNumber.empty()) {
        return;
    }

    OrderServiceResult<Order> result = orderService_.Release(orderNumber);
    if (result.Ok()) {
        view_.ShowReleaseResult(true, orderNumber, "");
    } else {
        view_.ShowReleaseResult(false, orderNumber, result.Error().message);
    }
    WaitForEnter();
}
