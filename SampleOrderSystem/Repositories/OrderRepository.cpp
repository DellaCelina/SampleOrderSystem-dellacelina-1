#include "OrderRepository.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

OrderRepository::OrderRepository(std::filesystem::path dataPath, std::filesystem::path schemaPath)
    : store_(dataPath.string(), LoadSchemaFromFile(schemaPath.string())),
      nextSequence_(1) {
    JsonValue data = store_.Load();
    for (const JsonValue& record : data.AsArray()) {
        cache_.push_back(Order::FromJson(record));
    }

    int maxSuffix = 0;
    for (const Order& order : cache_) {
        int suffix = ParseOrderNumberSuffix(order.orderNumber);
        maxSuffix = std::max(maxSuffix, suffix);
    }
    nextSequence_ = maxSuffix + 1;
}

int OrderRepository::ParseOrderNumberSuffix(const std::string& orderNumber) {
    const std::string prefix = "ORD-";
    if (orderNumber.size() != prefix.size() + 4 || orderNumber.compare(0, prefix.size(), prefix) != 0) {
        throw std::invalid_argument("OrderRepository: orderNumber \"" + orderNumber + "\" does not match ORD-####");
    }
    std::string digits = orderNumber.substr(prefix.size());
    for (char c : digits) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("OrderRepository: orderNumber \"" + orderNumber +
                                         "\" does not match ORD-####");
        }
    }
    return std::stoi(digits);
}

void OrderRepository::Persist() {
    JsonValue array = JsonValue::MakeArray();
    for (const Order& order : cache_) {
        array.Push(order.ToJson());
    }
    store_.Save(array);
}

std::string OrderRepository::NextOrderNumber() {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "ORD-%04d", nextSequence_);
    ++nextSequence_;
    return std::string(buffer);
}

void OrderRepository::Add(const Order& order) {
    for (const Order& existing : cache_) {
        if (existing.orderNumber == order.orderNumber) {
            throw std::invalid_argument("OrderRepository::Add: duplicate orderNumber \"" + order.orderNumber + "\"");
        }
    }
    cache_.push_back(order);
    Persist();
}

std::vector<Order> OrderRepository::FindAll() const {
    return cache_;
}

std::optional<Order> OrderRepository::FindByOrderNumber(const std::string& orderNumber) const {
    for (const Order& order : cache_) {
        if (order.orderNumber == orderNumber) {
            return order;
        }
    }
    return std::nullopt;
}

std::vector<Order> OrderRepository::FindByStatus(OrderStatus status) const {
    std::vector<Order> results;
    for (const Order& order : cache_) {
        if (order.status == status) {
            results.push_back(order);
        }
    }
    return results;
}

std::vector<Order> OrderRepository::FindBySampleId(const std::string& sampleId) const {
    std::vector<Order> results;
    for (const Order& order : cache_) {
        if (order.sampleId == sampleId) {
            results.push_back(order);
        }
    }
    return results;
}

void OrderRepository::UpdateStatus(const std::string& orderNumber, OrderStatus newStatus) {
    for (Order& order : cache_) {
        if (order.orderNumber == orderNumber) {
            order.status = newStatus;
            Persist();
            return;
        }
    }
    throw std::invalid_argument("OrderRepository::UpdateStatus: unknown orderNumber \"" + orderNumber + "\"");
}
