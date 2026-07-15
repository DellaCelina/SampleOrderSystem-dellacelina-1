#include "DummyDataGenerator.h"

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <optional>
#include <stdexcept>

#include "../Models/ProductionQueueEntry.h"

DummyDataGenerator::DummyDataGenerator(SampleRepository& sampleRepo,
                                       OrderRepository& orderRepo,
                                       ProductionQueueRepository& queueRepo,
                                       IClock& clock,
                                       unsigned int seed)
    : sampleRepo_(sampleRepo), orderRepo_(orderRepo), queueRepo_(queueRepo), clock_(clock), rng_(seed) {
    for (const Order& order : orderRepo_.FindAll()) {
        if (order.status == OrderStatus::Producing || order.status == OrderStatus::Confirmed) {
            runningClaims_[order.sampleId] += order.quantity;
        }
    }
}

OrderStatus DummyDataGenerator::PickStatusForSlot(int slotIndex, int totalCount) const {
    (void)totalCount;
    static const OrderStatus kSequence[5] = {
        OrderStatus::Reserved, OrderStatus::Confirmed, OrderStatus::Producing,
        OrderStatus::Released, OrderStatus::Rejected};
    return kSequence[slotIndex % 5];
}

int DummyDataGenerator::RawUnclaimedStock(const Sample& sample) const {
    int claimed = 0;
    auto it = runningClaims_.find(sample.sampleId);
    if (it != runningClaims_.end()) {
        claimed = it->second;
    }
    return sample.currentStock - claimed;
}

int DummyDataGenerator::UnclaimedStock(const Sample& sample) const {
    int unclaimed = RawUnclaimedStock(sample);
    return unclaimed > 0 ? unclaimed : 0;
}

void DummyDataGenerator::RecordClaim(const std::string& sampleId, int quantity) {
    runningClaims_[sampleId] += quantity;
}

void DummyDataGenerator::TopUpStockIfNeeded(Sample& sample, int minUnclaimed) {
    // Must use the raw (unclamped) deficit here, not UnclaimedStock()'s floor-0 view: when
    // claims already exceed stock by more than minUnclaimed, computing delta off the clamped
    // value under-tops-up (it only adds minUnclaimed, not enough to also cover the deficit
    // beyond zero), leaving the real unclaimed amount still <= 0 after "topping up".
    int rawUnclaimed = RawUnclaimedStock(sample);
    if (rawUnclaimed >= minUnclaimed) {
        return;
    }
    int delta = minUnclaimed - rawUnclaimed;
    sample.currentStock += delta;
    sampleRepo_.IncreaseStock(sample.sampleId, delta);
}

int DummyDataGenerator::PickQuantityWithGuaranteedStock(Sample& sample, int unclaimed) {
    if (unclaimed < 1) {
        TopUpStockIfNeeded(sample, unclaimed + RandomQuantity(1, 200));
        unclaimed = UnclaimedStock(sample);
    }
    return RandomQuantity(1, unclaimed);
}

std::string DummyDataGenerator::NextSampleId() {
    const std::string prefix = "SMP-";
    int maxSuffix = 0;
    for (const Sample& sample : sampleRepo_.FindAll()) {
        if (sample.sampleId.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        std::string digits = sample.sampleId.substr(prefix.size());
        bool allDigits = !digits.empty();
        for (char c : digits) {
            if (c < '0' || c > '9') {
                allDigits = false;
                break;
            }
        }
        if (!allDigits) {
            continue;
        }
        int suffix = std::stoi(digits);
        maxSuffix = std::max(maxSuffix, suffix);
    }

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "SMP-%04d", maxSuffix + 1);
    return std::string(buffer);
}

std::string DummyDataGenerator::RandomCustomerName() {
    static const std::vector<std::string> kNames = {
        "Acme Corp", "Globex", "Initech", "Umbrella Inc",
        "Stark Industries", "Wayne Enterprises", "Wonka Ltd", "Hooli"};
    std::uniform_int_distribution<size_t> dist(0, kNames.size() - 1);
    return kNames[dist(rng_)];
}

std::string DummyDataGenerator::RandomSampleName() {
    static const std::vector<std::string> kMaterials = {
        "GaAs", "Silicon", "InP", "SiC", "Sapphire", "GaN", "Germanium", "InGaAs"};
    static const std::vector<std::string> kForms = {"Wafer", "Ingot", "Die", "Chip", "Substrate"};
    std::uniform_int_distribution<size_t> materialDist(0, kMaterials.size() - 1);
    std::uniform_int_distribution<size_t> formDist(0, kForms.size() - 1);
    return kMaterials[materialDist(rng_)] + " " + kForms[formDist(rng_)];
}

int DummyDataGenerator::RandomQuantity(int lo, int hi) {
    if (lo > hi) {
        std::swap(lo, hi);
    }
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

std::vector<std::string> DummyDataGenerator::GenerateSamples(int count) {
    if (count <= 0) {
        return {};
    }

    std::vector<std::string> ids;
    ids.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Sample sample;
        sample.sampleId = NextSampleId();
        sample.name = RandomSampleName();
        sample.averageProductionTimeMinutes = RandomQuantity(1, 120);
        sample.yield = RandomQuantity(50, 100) / 100.0;
        sample.currentStock = RandomQuantity(0, 500);

        sampleRepo_.Add(sample);
        ids.push_back(sample.sampleId);
    }
    return ids;
}

std::vector<std::string> DummyDataGenerator::GenerateOrders(int count, const std::vector<std::string>& sampleIds) {
    if (sampleIds.empty() && count > 0) {
        throw std::invalid_argument("DummyDataGenerator::GenerateOrders: sampleIds must not be empty when count > 0");
    }
    if (count <= 0) {
        return {};
    }

    std::vector<int> slotOrder(static_cast<size_t>(count));
    std::iota(slotOrder.begin(), slotOrder.end(), 0);
    std::shuffle(slotOrder.begin(), slotOrder.end(), rng_);

    std::uniform_int_distribution<size_t> sampleDist(0, sampleIds.size() - 1);

    std::vector<std::string> orderNumbers;
    orderNumbers.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        int slot = slotOrder[static_cast<size_t>(i)];
        OrderStatus status = PickStatusForSlot(slot, count);

        const std::string& sampleId = sampleIds[sampleDist(rng_)];
        std::optional<Sample> sampleOpt = sampleRepo_.FindById(sampleId);
        if (!sampleOpt.has_value()) {
            throw std::invalid_argument("DummyDataGenerator::GenerateOrders: unknown sampleId \"" + sampleId + "\"");
        }
        Sample sample = *sampleOpt;

        Order order;
        order.orderNumber = orderRepo_.NextOrderNumber();
        order.sampleId = sampleId;
        order.customerName = RandomCustomerName();
        order.status = status;

        int unclaimed = UnclaimedStock(sample);

        switch (status) {
            case OrderStatus::Reserved:
            case OrderStatus::Rejected: {
                order.quantity = RandomQuantity(1, 200);
                break;
            }
            case OrderStatus::Confirmed: {
                order.quantity = PickQuantityWithGuaranteedStock(sample, unclaimed);
                RecordClaim(sampleId, order.quantity);
                break;
            }
            case OrderStatus::Producing: {
                order.quantity = unclaimed + RandomQuantity(1, 200);  // guarantees quantity > unclaimed
                int shortfall = ProductionService::ComputeShortfall(order.quantity, unclaimed);
                int actualProducedQuantity = ProductionService::ComputeActualQuantity(shortfall, sample.yield);

                std::vector<ProductionQueueEntry> existing = queueRepo_.FindAllInOrder();
                std::optional<std::chrono::system_clock::time_point> previousTailCompletion;
                if (!existing.empty()) {
                    previousTailCompletion = existing.back().expectedCompletionAt;
                }

                std::chrono::system_clock::time_point enqueuedAt = clock_.Now();
                int durationMinutes = ProductionService::ComputeProductionDurationMinutes(
                    actualProducedQuantity, sample.averageProductionTimeMinutes);
                std::chrono::system_clock::time_point expectedCompletionAt =
                    ProductionService::ComputeCompletionTime(enqueuedAt, previousTailCompletion, durationMinutes);

                ProductionQueueEntry entry;
                entry.orderNumber = order.orderNumber;
                entry.sampleId = sampleId;
                entry.shortfallQuantity = shortfall;
                entry.actualProducedQuantity = actualProducedQuantity;
                entry.enqueuedAt = enqueuedAt;
                entry.expectedCompletionAt = expectedCompletionAt;
                queueRepo_.Enqueue(entry);

                RecordClaim(sampleId, order.quantity);
                break;
            }
            case OrderStatus::Released: {
                order.quantity = PickQuantityWithGuaranteedStock(sample, unclaimed);
                sampleRepo_.DecreaseStock(sampleId, order.quantity);
                break;
            }
        }

        orderRepo_.Add(order);
        orderNumbers.push_back(order.orderNumber);
    }

    return orderNumbers;
}

void DummyDataGenerator::GenerateAll(const DummyDataOptions& options) {
    std::vector<std::string> sampleIds = GenerateSamples(options.sampleCount);
    GenerateOrders(options.orderCount, sampleIds);
}
