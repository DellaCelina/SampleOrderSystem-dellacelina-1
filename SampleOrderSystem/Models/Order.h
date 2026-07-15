#pragma once
#include <string>
#include "../Json/JsonValue.h"


enum class OrderStatus { Reserved, Confirmed, Producing, Released, Rejected };

// Canonical string forms are the upper-case status names ("RESERVED", "CONFIRMED", "PRODUCING",
// "RELEASED", "REJECTED") -- matching the requirement doc's own status vocabulary -- used as the
// JSON representation of the "status" field.
std::string OrderStatusToString(OrderStatus status);

// Throws std::invalid_argument (message includes the offending string) for any input that isn't
// exactly one of the five canonical strings above (case-sensitive -- "reserved" is rejected, not
// silently normalized).
OrderStatus OrderStatusFromString(const std::string& text);

struct Order {
    std::string orderNumber;      // "ORD-####", not validated for format/uniqueness here
    std::string sampleId;
    std::string customerName;
    int quantity;
    OrderStatus status;

    JsonValue ToJson() const;
    static Order FromJson(const JsonValue& json);
};
