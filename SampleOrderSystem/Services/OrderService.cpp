#include "OrderService.h"

#include <algorithm>

#include "../Models/Sample.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/SampleRepository.h"

OrderService::OrderService(OrderRepository& orderRepository,
                           SampleRepository& sampleRepository,
                           ProductionService& productionService)
    : orderRepository_(orderRepository),
      sampleRepository_(sampleRepository),
      productionService_(productionService) {}

OrderServiceResult<Order> OrderService::SubmitOrder(const std::string& sampleId,
                                                     const std::string& customerName,
                                                     int quantity) {
    if (quantity <= 0) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::InvalidQuantity,
                                                    "quantity must be positive");
    }

    std::optional<Sample> sample = sampleRepository_.FindById(sampleId);
    if (!sample.has_value()) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::SampleNotFound,
                                                    "unknown sampleId \"" + sampleId + "\"");
    }

    Order order;
    order.orderNumber = orderRepository_.NextOrderNumber();
    order.sampleId = sampleId;
    order.customerName = customerName;
    order.quantity = quantity;
    order.status = OrderStatus::Reserved;

    orderRepository_.Add(order);

    return OrderServiceResult<Order>::Success(order);
}

std::vector<Order> OrderService::ListPendingApprovals() const {
    std::vector<Order> result;
    for (const Order& order : orderRepository_.FindAll()) {
        if (order.status == OrderStatus::Reserved) {
            result.push_back(order);
        }
    }
    return result;
}

int OrderService::ComputeUnclaimedStock(const std::string& sampleId) const {
    std::optional<Sample> sample = sampleRepository_.FindById(sampleId);
    int currentStock = sample.has_value() ? sample->currentStock : 0;

    int claimed = 0;
    for (const Order& order : orderRepository_.FindBySampleId(sampleId)) {
        if (order.status == OrderStatus::Producing || order.status == OrderStatus::Confirmed) {
            claimed += order.quantity;
        }
    }

    int unclaimed = currentStock - claimed;
    return unclaimed > 0 ? unclaimed : 0;
}

OrderServiceResult<Order> OrderService::FindOrderOrFail(const std::string& orderNumber) const {
    std::optional<Order> maybeOrder = orderRepository_.FindByOrderNumber(orderNumber);
    if (!maybeOrder.has_value()) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::OrderNotFound,
                                                    "unknown orderNumber \"" + orderNumber + "\"");
    }
    return OrderServiceResult<Order>::Success(*maybeOrder);
}

OrderServiceResult<Order> OrderService::Approve(const std::string& orderNumber, IClock& clock) {
    productionService_.SettleDueEntries(clock);

    OrderServiceResult<Order> found = FindOrderOrFail(orderNumber);
    if (!found.Ok()) {
        return found;
    }

    Order order = found.Value();
    if (order.status != OrderStatus::Reserved) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::InvalidStatusForApproval,
                                                    "order \"" + orderNumber + "\" is not Reserved");
    }

    int unclaimed = ComputeUnclaimedStock(order.sampleId);

    if (unclaimed >= order.quantity) {
        orderRepository_.UpdateStatus(orderNumber, OrderStatus::Confirmed);
        order.status = OrderStatus::Confirmed;
        return OrderServiceResult<Order>::Success(order);
    }

    int shortfall = ProductionService::ComputeShortfall(order.quantity, unclaimed);
    productionService_.Enqueue(orderNumber, order.sampleId, shortfall, clock);
    orderRepository_.UpdateStatus(orderNumber, OrderStatus::Producing);
    order.status = OrderStatus::Producing;
    return OrderServiceResult<Order>::Success(order);
}

OrderServiceResult<Order> OrderService::Reject(const std::string& orderNumber) {
    OrderServiceResult<Order> found = FindOrderOrFail(orderNumber);
    if (!found.Ok()) {
        return found;
    }

    Order order = found.Value();
    if (order.status != OrderStatus::Reserved) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::InvalidStatusForRejection,
                                                    "order \"" + orderNumber + "\" is not Reserved");
    }

    orderRepository_.UpdateStatus(orderNumber, OrderStatus::Rejected);
    order.status = OrderStatus::Rejected;
    return OrderServiceResult<Order>::Success(order);
}

OrderServiceResult<Order> OrderService::Release(const std::string& orderNumber) {
    OrderServiceResult<Order> found = FindOrderOrFail(orderNumber);
    if (!found.Ok()) {
        return found;
    }

    Order order = found.Value();
    if (order.status != OrderStatus::Confirmed) {
        return OrderServiceResult<Order>::Failure(OrderServiceErrorCode::InvalidStatusForRelease,
                                                    "order \"" + orderNumber + "\" is not Confirmed");
    }

    sampleRepository_.DecreaseStock(order.sampleId, order.quantity);
    orderRepository_.UpdateStatus(orderNumber, OrderStatus::Released);
    order.status = OrderStatus::Released;
    return OrderServiceResult<Order>::Success(order);
}
