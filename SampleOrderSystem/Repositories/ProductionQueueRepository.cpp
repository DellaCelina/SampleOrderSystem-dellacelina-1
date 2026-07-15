#include "ProductionQueueRepository.h"

#include <stdexcept>

ProductionQueueRepository::ProductionQueueRepository(std::filesystem::path dataPath,
                                                      std::filesystem::path schemaPath)
    : store_(dataPath.string(), LoadSchemaFromFile(schemaPath.string())) {
    JsonValue data = store_.Load();
    for (const JsonValue& record : data.AsArray()) {
        cache_.push_back(ProductionQueueEntry::FromJson(record));
    }
}

void ProductionQueueRepository::Persist() {
    JsonValue array = JsonValue::MakeArray();
    for (const ProductionQueueEntry& entry : cache_) {
        array.Push(entry.ToJson());
    }
    store_.Save(array);
}

void ProductionQueueRepository::Enqueue(const ProductionQueueEntry& entry) {
    cache_.push_back(entry);
    Persist();
}

std::vector<ProductionQueueEntry> ProductionQueueRepository::FindAllInOrder() const {
    return cache_;
}

std::optional<ProductionQueueEntry> ProductionQueueRepository::PeekHead() const {
    if (cache_.empty()) {
        return std::nullopt;
    }
    return cache_.front();
}

void ProductionQueueRepository::Remove(const std::string& orderNumber) {
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->orderNumber == orderNumber) {
            cache_.erase(it);
            Persist();
            return;
        }
    }
    throw std::invalid_argument("ProductionQueueRepository::Remove: unknown orderNumber \"" + orderNumber + "\"");
}
