#include "Order.h"

#include <stdexcept>

std::string OrderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::Reserved:
            return "RESERVED";
        case OrderStatus::Confirmed:
            return "CONFIRMED";
        case OrderStatus::Producing:
            return "PRODUCING";
        case OrderStatus::Released:
            return "RELEASED";
        case OrderStatus::Rejected:
            return "REJECTED";
    }
    throw std::invalid_argument("OrderStatusToString: unrecognized OrderStatus enum value");
}

OrderStatus OrderStatusFromString(const std::string& text) {
    if (text == "RESERVED") return OrderStatus::Reserved;
    if (text == "CONFIRMED") return OrderStatus::Confirmed;
    if (text == "PRODUCING") return OrderStatus::Producing;
    if (text == "RELEASED") return OrderStatus::Released;
    if (text == "REJECTED") return OrderStatus::Rejected;
    throw std::invalid_argument("OrderStatusFromString: unrecognized status string \"" + text + "\"");
}

namespace {

const JsonValue& RequireField(const JsonValue& json, const std::string& key) {
    if (!json.Has(key)) {
        throw std::invalid_argument("Order::FromJson: missing required field \"" + key + "\"");
    }
    return json.Get(key);
}

}  // namespace

JsonValue Order::ToJson() const {
    JsonValue json = JsonValue::MakeObject();
    json.Set("orderNumber", orderNumber);
    json.Set("sampleId", sampleId);
    json.Set("customerName", customerName);
    json.Set("quantity", quantity);
    json.Set("status", OrderStatusToString(status));
    return json;
}

Order Order::FromJson(const JsonValue& json) {
    Order order;

    const JsonValue& orderNumber = RequireField(json, "orderNumber");
    if (!orderNumber.IsString()) {
        throw std::invalid_argument("Order::FromJson: \"orderNumber\" must be a string");
    }
    order.orderNumber = orderNumber.AsString();

    const JsonValue& sampleId = RequireField(json, "sampleId");
    if (!sampleId.IsString()) {
        throw std::invalid_argument("Order::FromJson: \"sampleId\" must be a string");
    }
    order.sampleId = sampleId.AsString();

    const JsonValue& customerName = RequireField(json, "customerName");
    if (!customerName.IsString()) {
        throw std::invalid_argument("Order::FromJson: \"customerName\" must be a string");
    }
    order.customerName = customerName.AsString();

    const JsonValue& quantity = RequireField(json, "quantity");
    if (!quantity.IsNumber()) {
        throw std::invalid_argument("Order::FromJson: \"quantity\" must be a number");
    }
    order.quantity = static_cast<int>(quantity.AsNumber());

    const JsonValue& status = RequireField(json, "status");
    if (!status.IsString()) {
        throw std::invalid_argument("Order::FromJson: \"status\" must be a string");
    }
    order.status = OrderStatusFromString(status.AsString());

    return order;
}
