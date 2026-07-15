#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Core/IClock.h"
#include "../Models/Order.h"
#include "ProductionService.h"

class OrderRepository;
class SampleRepository;

enum class OrderServiceErrorCode {
    SampleNotFound,
    InvalidQuantity,
    OrderNotFound,
    InvalidStatusForApproval,
    InvalidStatusForRejection,
    InvalidStatusForRelease,
};

struct OrderServiceError {
    OrderServiceErrorCode code;
    std::string message;  // human-readable, for eventual console display
};

template <typename T>
class OrderServiceResult {
public:
    static OrderServiceResult Success(T value) {
        OrderServiceResult result;
        result.ok_ = true;
        result.value_ = std::move(value);
        return result;
    }

    static OrderServiceResult Failure(OrderServiceErrorCode code, std::string message) {
        OrderServiceResult result;
        result.ok_ = false;
        result.error_ = OrderServiceError{code, std::move(message)};
        return result;
    }

    bool Ok() const { return ok_; }
    explicit operator bool() const { return Ok(); }

    const T& Value() const { return *value_; }
    const OrderServiceError& Error() const { return *error_; }

private:
    bool ok_ = false;
    std::optional<T> value_;
    std::optional<OrderServiceError> error_;
};

// Order-lifecycle orchestration: submit, list pending, approve, reject, release.
// See docs/impl/s-semi/phase-07-services/DETAIL-order-service.md for the full contract.
class OrderService {
public:
    OrderService(OrderRepository& orderRepository,
                 SampleRepository& sampleRepository,
                 ProductionService& productionService);

    OrderServiceResult<Order> SubmitOrder(const std::string& sampleId,
                                           const std::string& customerName,
                                           int quantity);

    std::vector<Order> ListPendingApprovals() const;

    // Orders currently eligible for Release() (status == Confirmed), for the
    // "출고 처리" (release) screen to show before prompting for an order
    // number -- mirrors ListPendingApprovals' contract (no settlement sweep;
    // callers that need matured PRODUCING->CONFIRMED transitions reflected
    // must settle first, same as ListPendingApprovals).
    std::vector<Order> ListReleasable() const;

    OrderServiceResult<Order> Approve(const std::string& orderNumber, IClock& clock);

    OrderServiceResult<Order> Reject(const std::string& orderNumber);

    OrderServiceResult<Order> Release(const std::string& orderNumber);

private:
    int ComputeUnclaimedStock(const std::string& sampleId) const;

    // Looks up orderNumber; returns OrderNotFound failure if absent, else Success(order).
    OrderServiceResult<Order> FindOrderOrFail(const std::string& orderNumber) const;

    OrderRepository& orderRepository_;
    SampleRepository& sampleRepository_;
    ProductionService& productionService_;
};
