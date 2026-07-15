#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/IClock.h"
#include "../Models/Order.h"
#include "../Models/Sample.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/ProductionQueueRepository.h"
#include "../Repositories/SampleRepository.h"
#include "ProductionService.h"

struct DummyDataOptions {
    int sampleCount = 10;
    int orderCount = 20;
};

// Generates realistic, invariant-respecting random Sample/Order/queue records
// directly through the repositories (bypassing OrderService/ProductionService
// orchestration), for use by a later --dummy-data CLI/controller phase.
class DummyDataGenerator {
public:
    // seed is mandatory (no default) so tests are always deterministic.
    DummyDataGenerator(SampleRepository& sampleRepo,
                        OrderRepository& orderRepo,
                        ProductionQueueRepository& queueRepo,
                        IClock& clock,
                        unsigned int seed);

    std::vector<std::string> GenerateSamples(int count);

    std::vector<std::string> GenerateOrders(int count, const std::vector<std::string>& sampleIds);

    void GenerateAll(const DummyDataOptions& options = {});

private:
    OrderStatus PickStatusForSlot(int slotIndex, int totalCount) const;
    int UnclaimedStock(const Sample& sample) const;
    void RecordClaim(const std::string& sampleId, int quantity);
    void TopUpStockIfNeeded(Sample& sample, int minUnclaimed);
    // Tops up stock if needed so at least 1 unit is unclaimed, then returns a
    // random quantity in [1, unclaimed]. Shared by the Confirmed/Released branches,
    // which differ only in what they do with the picked quantity afterward.
    int PickQuantityWithGuaranteedStock(Sample& sample, int unclaimed);
    std::string NextSampleId();
    std::string RandomCustomerName();
    std::string RandomSampleName();
    int RandomQuantity(int lo, int hi);

    SampleRepository& sampleRepo_;
    OrderRepository& orderRepo_;
    ProductionQueueRepository& queueRepo_;
    IClock& clock_;
    std::mt19937 rng_;
    std::unordered_map<std::string, int> runningClaims_;
};
