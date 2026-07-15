#pragma once
#include <optional>
#include <chrono>
#include <string>

#include "../Core/IClock.h"
#include "../Models/ProductionQueueEntry.h"
#include "../Repositories/SampleRepository.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/ProductionQueueRepository.h"


class ProductionService {
public:
    ProductionService(SampleRepository& samples,
                       OrderRepository& orders,
                       ProductionQueueRepository& queue);

    // ---- Pure, stateless math. No repository/file access. Safe to call
    // standalone (this is the contract phase-9's DummyDataGenerator relies on
    // to keep its generated Producing orders' queue entries consistent with
    // what a real Approve() would have produced). ----

    // max(0, requestedQuantity - unclaimedStock). Caller (OrderService) is
    // responsible for the max(0,...) clamp on unclaimedStock itself per the
    // architecture's unclaimed-stock formula (Key Design Decision #2); this
    // function additionally clamps its own result to >= 0 defensively so it
    // can never return a negative shortfall even if called with an
    // out-of-contract negative unclaimedStock.
    static int ComputeShortfall(int requestedQuantity, int unclaimedStock);

    // ceil(shortfall / yield). shortfall <= 0 => returns 0 (no production
    // needed). yield is assumed in (0, 1] per Sample's schema constraint;
    // this function does not itself validate that range.
    static int ComputeActualQuantity(int shortfall, double yield);

    // ceil(actualProducedQuantity * averageProductionTimeMinutes). averageProductionTimeMinutes
    // may be a non-integral positive real number (e.g. 2.5 minutes/unit); the product is rounded
    // UP to the next whole minute (never down) since std::chrono::minutes -- what this duration
    // ultimately feeds into via ComputeCompletionTime -- has no sub-minute precision, and rounding
    // down would let production appear to finish before it actually would.
    static int ComputeProductionDurationMinutes(int actualProducedQuantity,
                                                 double averageProductionTimeMinutes);

    // FIFO chain rule: max(enqueuedAt, previousTailCompletion.value_or(enqueuedAt))
    // + minutes(durationMinutes). previousTailCompletion is the
    // expectedCompletionAt of the current last entry in the queue, or
    // std::nullopt if the queue is empty (in which case the max(...) term
    // degenerates to enqueuedAt itself). Callers (Enqueue below, and
    // phase-9) are responsible for having already settled/pruned any
    // already-due tail entry before reading it, so previousTailCompletion is
    // never itself in the past relative to enqueuedAt in the real-Approve
    // path -- but this function does not assume or enforce that; it is a
    // pure max/add and behaves correctly (returns enqueuedAt + duration)
    // even if a stale/past previousTailCompletion is passed in.
    static std::chrono::system_clock::time_point ComputeCompletionTime(
        std::chrono::system_clock::time_point enqueuedAt,
        std::optional<std::chrono::system_clock::time_point> previousTailCompletion,
        int durationMinutes);

    // ---- Stateful operations (repository/file access) ----

    // Looks up the sample (throws std::invalid_argument if sampleId is
    // unknown -- defensive; real callers validate existence earlier),
    // computes actualProducedQuantity/duration via the pure functions above,
    // reads the current queue's last entry (via
    // ProductionQueueRepository) for the chain calc, builds a
    // ProductionQueueEntry, appends it (FIFO = append order, per Key Design
    // Decision #6) and persists via ProductionQueueRepository, and
    // returns the created entry by value. Does NOT mutate sample stock or
    // order status -- stock/status only change on settlement. Does NOT
    // decide Confirmed-vs-Producing; that decision and the
    // unclaimed-stock-based shortfall computation stay in OrderService
    // (phase-7); this method receives an already-computed shortfall.
    ProductionQueueEntry Enqueue(const std::string& orderNumber,
                                          const std::string& sampleId,
                                          int shortfallQuantity,
                                          IClock& clock);

    // The single lazy-reconciliation entry point. Idempotent, cheap no-op
    // when nothing is due. See "SettleDueEntries behavior" in phase-6's
    // DETAIL.md.
    void SettleDueEntries(IClock& clock);

private:
    SampleRepository& m_samples;
    OrderRepository& m_orders;
    ProductionQueueRepository& m_queue;
};
