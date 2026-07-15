#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../Models/ProductionQueueEntry.h"
#include "../Persistence/JsonFileStore.h"

class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(
        std::filesystem::path dataPath = "data/production_queue.json",
        std::filesystem::path schemaPath = "schema/production_queue.schema.json");

    void Enqueue(const ProductionQueueEntry& entry);

    std::vector<ProductionQueueEntry> FindAllInOrder() const;

    std::optional<ProductionQueueEntry> PeekHead() const;

    // Throws std::invalid_argument if orderNumber not found in the queue.
    void Remove(const std::string& orderNumber);

private:
    JsonFileStore store_;
    std::vector<ProductionQueueEntry> cache_;

    void Persist();
};
