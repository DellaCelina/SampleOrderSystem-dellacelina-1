#include <gtest/gtest.h>

#include "Models/Order.h"
#include "Json/JsonValue.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

JsonValue MakeValidOrderJson(const std::string& status = "RESERVED") {
    JsonValue json = JsonValue::MakeObject();
    json.Set("orderNumber", "ORD-0001");
    json.Set("sampleId", "SMP-001");
    json.Set("customerName", "Acme Corp");
    json.Set("quantity", 10);
    json.Set("status", status);
    return json;
}

}  // namespace

TEST(OrderTest, OrderStatusToStringOrderStatusFromStringRoundTripForAllFiveEnumValues) {
    const std::vector<std::pair<OrderStatus, std::string>> cases = {
        {OrderStatus::Reserved, "RESERVED"},
        {OrderStatus::Confirmed, "CONFIRMED"},
        {OrderStatus::Producing, "PRODUCING"},
        {OrderStatus::Released, "RELEASED"},
        {OrderStatus::Rejected, "REJECTED"},
    };

    for (const auto& [status, text] : cases) {
        EXPECT_EQ(OrderStatusToString(status), text);
        EXPECT_EQ(OrderStatusFromString(text), status);
    }
}

TEST(OrderTest, OrderStatusFromStringThrowsOnAnUnrecognizedString_UnknownValue) {
    EXPECT_THROW(OrderStatusFromString("UNKNOWN"), std::invalid_argument);
}

TEST(OrderTest, OrderStatusFromStringThrowsOnAnUnrecognizedString_EmptyString) {
    EXPECT_THROW(OrderStatusFromString(""), std::invalid_argument);
}

TEST(OrderTest, OrderStatusFromStringThrowsOnAnUnrecognizedString_LowerCaseVariantIsNotNormalizedCaseSensitive) {
    EXPECT_THROW(OrderStatusFromString("reserved"), std::invalid_argument);
}

TEST(OrderTest, OrderRoundTripsThroughToJsonFromJsonForEachStatusValue) {
    const std::vector<std::pair<OrderStatus, std::string>> cases = {
        {OrderStatus::Reserved, "RESERVED"},
        {OrderStatus::Confirmed, "CONFIRMED"},
        {OrderStatus::Producing, "PRODUCING"},
        {OrderStatus::Released, "RELEASED"},
        {OrderStatus::Rejected, "REJECTED"},
    };

    for (const auto& [status, text] : cases) {
        Order order;
        order.orderNumber = "ORD-0007";
        order.sampleId = "SMP-007";
        order.customerName = "Beta Inc";
        order.quantity = 5;
        order.status = status;

        const JsonValue json = order.ToJson();
        EXPECT_EQ(json.Get("status").AsString(), text);

        const Order roundTripped = Order::FromJson(json);
        EXPECT_EQ(roundTripped.orderNumber, order.orderNumber);
        EXPECT_EQ(roundTripped.sampleId, order.sampleId);
        EXPECT_EQ(roundTripped.customerName, order.customerName);
        EXPECT_EQ(roundTripped.quantity, order.quantity);
        EXPECT_EQ(roundTripped.status, order.status);
    }
}

TEST(OrderTest, OrderFromJsonThrowsWhenARequiredFieldIsMissing) {
    const std::vector<std::string> requiredFields = {
        "orderNumber", "sampleId", "customerName", "quantity", "status"};

    for (const auto& missingField : requiredFields) {
        JsonValue full = MakeValidOrderJson();
        JsonValue withoutField = JsonValue::MakeObject();
        for (const auto& field : requiredFields) {
            if (field != missingField) {
                withoutField.Set(field, full.Get(field));
            }
        }
        EXPECT_THROW(Order::FromJson(withoutField), std::invalid_argument);
    }
}

TEST(OrderTest, OrderFromJsonThrowsWhenStatusIsPresentButNotARecognizedValue) {
    JsonValue json = MakeValidOrderJson("NOT_A_STATUS");

    EXPECT_THROW(Order::FromJson(json), std::invalid_argument);
}

TEST(OrderTest, OrderFromJsonThrowsWhenAFieldHasTheWrongJsonType) {
    JsonValue json = MakeValidOrderJson();
    json.Set("quantity", "not-a-number");

    EXPECT_THROW(Order::FromJson(json), std::invalid_argument);
}
