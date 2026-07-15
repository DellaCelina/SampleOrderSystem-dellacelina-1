#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "../Persistence/JsonFileStore.h"

class OrderRepository {
public:
    explicit OrderRepository(std::filesystem::path dataPath = "data/orders.json",
                              std::filesystem::path schemaPath = "schema/order.schema.json");

    // Returns "ORD-####" and advances the in-memory counter. Does not persist.
    std::string NextOrderNumber();

    // Throws std::invalid_argument if orderNumber already exists.
    void Add(const Order& order);

    std::vector<Order> FindAll() const;
    std::optional<Order> FindByOrderNumber(const std::string& orderNumber) const;
    std::vector<Order> FindByStatus(OrderStatus status) const;
    std::vector<Order> FindBySampleId(const std::string& sampleId) const;

    // Throws std::invalid_argument if orderNumber not found.
    void UpdateStatus(const std::string& orderNumber, OrderStatus newStatus);

private:
    JsonFileStore store_;
    std::vector<Order> cache_;
    int nextSequence_;

    void Persist();
    static int ParseOrderNumberSuffix(const std::string& orderNumber);
};
