# Phase 6: ProductionService: shortfall math, FIFO completion time, lazy settlement

**Depends on:** Phase 1 (test-scaffolding-clock), Phase 5 (repositories)
**Touches:** `SampleOrderSystem/Services/ProductionService.h`, `SampleOrderSystem/Services/ProductionService.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement ProductionService's ComputeShortfall, ComputeActualQuantity (ceil(shortfall/yield)), ComputeCompletionTime (the FIFO chain rule max(enqueue_time, prevCompletion)+duration against the current queue tail), Enqueue (persists a new ProductionQueueEntry), and SettleDueEntries(IClock&) — the single lazy-reconciliation entry point that sweeps ProductionQueueRepository for entries whose expectedCompletionAt has passed, increments sample stock by actualProducedQuantity, and flips the matching order Producing->Confirmed. ComputeShortfall/ComputeActualQuantity/ComputeCompletionTime must be exposed as pure, independently-callable functions/static methods on ProductionService (not private helpers folded into Enqueue), because phase-9 (DummyDataGenerator) needs to call this exact same math to keep generated Producing orders' queue entries consistent with real orders' — see phase-9. Tested with FakeClock for deterministic multi-entry FIFO chaining and idempotent no-op sweeps. This is the core production-domain logic that OrderService, MonitoringService, ProductionLineViewService, and DummyDataGenerator will all call into, so it must land before phase-7/phase-8/phase-9. Add Services/ProductionService.h/.cpp to SampleOrderSystemTests.vcxproj.

## Detail

## Scope

Implement `ProductionService` — the sole owner of shortfall/production-quantity/FIFO-completion-time math and the single lazy-settlement sweep (`SettleDueEntries`). This phase does **not** touch `OrderService`, `MonitoringService`, or `ProductionLineViewService` (phase-7/8) — it only builds the class those phases and phase-9 (`DummyDataGenerator`) will call into. Depends on phase-1 (`Core::IClock`) and phase-4 (`Models::Sample`, `Models::Order`/`OrderStatus`, `Models::ProductionQueueEntry`) transitively via phase-5 (`SampleRepository`, `OrderRepository`, `ProductionQueueRepository`), which this phase depends on directly. If phase-5's actual repository method names differ from the ones assumed below, adapt the call sites to match phase-5's real signatures — the important, fixed thing is `ProductionService`'s own public interface, not the exact repository API it happens to call.

## Public interface (`SampleOrderSystem/Services/ProductionService.h`)

```cpp
#pragma once
#include <optional>
#include "Core/IClock.h"
#include "Models/ProductionQueueEntry.h"
#include "Repositories/SampleRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"

namespace Services {

class ProductionService {
public:
    ProductionService(Repositories::SampleRepository& samples,
                       Repositories::OrderRepository& orders,
                       Repositories::ProductionQueueRepository& queue);

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

    // actualProducedQuantity * averageProductionTimeMinutes.
    static int ComputeProductionDurationMinutes(int actualProducedQuantity,
                                                 int averageProductionTimeMinutes);

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
    static Core::TimePoint ComputeCompletionTime(
        Core::TimePoint enqueuedAt,
        std::optional<Core::TimePoint> previousTailCompletion,
        int durationMinutes);

    // ---- Stateful operations (repository/file access) ----

    // Looks up the sample (throws std::invalid_argument if sampleId is
    // unknown -- defensive; real callers validate existence earlier),
    // computes actualProducedQuantity/duration via the pure functions above,
    // reads the current queue's last entry (via
    // ProductionQueueRepository) for the chain calc, builds a
    // ProductionQueueEntry, appends it (FIFO = append order, per Key Design
    // Decision #6) and persists via ProductionQueueRepository::Add, and
    // returns the created entry by value. Does NOT mutate sample stock or
    // order status -- stock/status only change on settlement. Does NOT
    // decide Confirmed-vs-Producing; that decision and the
    // unclaimed-stock-based shortfall computation stay in OrderService
    // (phase-7); this method receives an already-computed shortfall.
    Models::ProductionQueueEntry Enqueue(const std::string& orderNumber,
                                          const std::string& sampleId,
                                          int shortfallQuantity,
                                          Core::IClock& clock);

    // The single lazy-reconciliation entry point. Idempotent, cheap no-op
    // when nothing is due. See "SettleDueEntries behavior" below.
    void SettleDueEntries(Core::IClock& clock);

private:
    Repositories::SampleRepository& m_samples;
    Repositories::OrderRepository& m_orders;
    Repositories::ProductionQueueRepository& m_queue;
};

} // namespace Services
```

Note on `Enqueue`'s signature: it deliberately takes an already-computed `shortfallQuantity` rather than `(requestedQuantity, unclaimedStock)`, because `OrderService::Approve` (phase-7) computes shortfall from its own unclaimed-stock formula (which needs order/sample data `Enqueue` has no reason to see) and only needs `ProductionService` to turn that shortfall into an actual-quantity + completion-time + persisted entry. `ComputeShortfall` is exposed separately as a pure function for phase-9 and for direct unit testing, not because `Enqueue` calls it internally.

## `SettleDueEntries` behavior (the core of this phase)

1. Load all entries from `ProductionQueueRepository` in their stored (FIFO/array) order.
2. Partition into "due" (`expectedCompletionAt <= clock.Now()`) and "not due". Because every entry's `expectedCompletionAt` is non-decreasing along the FIFO chain (each is computed as `max(enqueuedAt, prevCompletion) + duration` with `duration > 0`), in practice due entries are always a prefix of the array — but implement this as a filter over *all* entries (do not assume-and-skip-scanning past the first not-due one), since that is what the requirement text ("sweep the file for entries whose completion time has passed") literally asks for and it costs nothing extra at this data scale.
3. For each due entry, in FIFO order:
   - Add `actualProducedQuantity` to the corresponding sample's `currentStock` (via `SampleRepository`, keyed by `sampleId`).
   - Transition the corresponding order (`orderNumber`) from `Producing` to `Confirmed` (via `OrderRepository`). If the order is not found or is not currently `Producing` (defensive — should not happen given the invariant that only `Producing` orders have a live queue entry), skip the status mutation for that entry but still remove it from the queue and still credit the stock, and do not throw — this keeps settlement total and non-blocking; note this as a defensive branch in a comment, not a designed-for path.
4. Persist: write back the updated sample stock, the updated order statuses, and the pruned queue (containing only the not-due entries, in their original relative order) — all before returning.
5. If no entries are due, this is a true no-op: no file writes are forced (or, if `JsonFileStore`'s save is unconditional/cheap, at least no data changes) and repeated calls with an unchanged clock produce identical state.

## Test list (Catch2, using `FakeClock`)

Pure math:
- `ComputeShortfall`: unclaimed >= requested => 0; unclaimed < requested => exact difference; unclaimed == 0 => shortfall == requested.
- `ComputeActualQuantity`: shortfall 0 => 0; yield 1.0 => actual == shortfall exactly; yield 0.9, shortfall 90 => 100 (exact division, no rounding needed); yield 0.9, shortfall 10 => 12 (ceil(11.11) = 12, confirms rounding up not down/truncation); yield 0.1, shortfall 1 => 10.
- `ComputeProductionDurationMinutes`: straightforward multiplication, at least two value pairs including a case where actualProducedQuantity == 0 => 0.
- `ComputeCompletionTime`: (a) empty-queue case (`previousTailCompletion = nullopt`) => `enqueuedAt + duration`; (b) previous tail completion later than `enqueuedAt` => result is `previousTailCompletion + duration` (chaining behavior); (c) previous tail completion earlier than `enqueuedAt` (e.g., because settlement had already advanced past it) => result is `enqueuedAt + duration`, not anchored on the stale earlier value; (d) previous tail completion exactly equal to `enqueuedAt` => result is `enqueuedAt + duration` (boundary, `max` picks either since they're equal).

`Enqueue`:
- Enqueuing against an empty queue produces an entry whose `expectedCompletionAt` equals `enqueuedAt(now) + avgTime*actualQty` and whose `enqueuedAt` equals the clock's current time.
- Enqueuing a second time (queue now has one entry) chains off the first entry's `expectedCompletionAt` rather than `Now()` — verifies `Enqueue` actually reads the queue tail, not just calls the pure function with `nullopt` every time.
- The persisted entry's `shortfallQuantity`/`actualProducedQuantity`/`orderNumber`/`sampleId` round-trip correctly through the repository (read it back after `Enqueue` and compare field-for-field).
- Enqueuing for an unknown `sampleId` throws (exact exception type left to implementer's repository-layer convention, but must not silently create a bogus entry).

`SettleDueEntries`:
- Empty queue => no-op (no throw, no state change).
- Single entry, clock before `expectedCompletionAt` => remains in queue, order status untouched, sample stock untouched.
- Single entry, clock exactly at `expectedCompletionAt` => settles (boundary case: `<=`, not `<`).
- Single entry, clock after `expectedCompletionAt` => settles: entry removed from queue, sample stock increased by exactly `actualProducedQuantity`, order flips `Producing -> Confirmed`.
- Multiple entries (different orders, same or different samples) where only some are due at the current clock time => only the due ones settle; the remaining entries stay in the queue in their original relative order (FIFO order preserved for the untouched tail).
- Multiple due entries at once (clock advanced past several) => all settle correctly and independently (each order's own status flips, each sample's own stock is credited — verify no cross-contamination between two entries referencing different samples).
- Idempotency: call `SettleDueEntries` twice with the same clock time (no advance between calls) after one entry became due => the second call is a true no-op — assert stock is *not* double-credited and the order does not error/re-transition on the second call.
- Multi-step FIFO scenario mirroring the 50-then-100-then-100 acceptance example: two entries enqueued back-to-back for the same sample via `Enqueue` (second chains off first's completion time); advance the fake clock past only the first entry's completion; `SettleDueEntries` settles exactly the first (stock credited once, first order `Confirmed`) while the second remains `Producing` and in the queue; advance further past the second's completion; a subsequent `SettleDueEntries` call settles the second.

## Build wiring

- Add `SampleOrderSystem/Services/ProductionService.h` and `.cpp` as new `ClInclude`/`ClCompile` items in `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`, using the same relative-path-into-`SampleOrderSystem`-tree pattern already established for phase-1/phase-5's model/repository files (per ARCHITECTURE.md's "Build/test wiring" section: the test project compiles these sources directly into its own binary rather than linking a shared library).
- These two files must also be added to `SampleOrderSystem.vcxproj`'s own `ClInclude`/`ClCompile` items so the production console binary actually compiles `ProductionService` — even though that file isn't listed under this phase's declared `touches`, it is necessary for the app to build once this phase's callers (phase-7/8) start referencing `ProductionService`. If phase-5 or an earlier phase already established a convention of adding new Service/Repository files to `SampleOrderSystem.vcxproj` as part of the phase that introduces them, follow that same convention here; if in doubt, add the entries to both `.vcxproj` files in this phase rather than deferring it, since leaving `SampleOrderSystem.vcxproj` un-updated would silently break the app build.

## Design notes / things intentionally left to this phase's implementer

- `Core::TimePoint` and `IClock` come from phase-1; use whatever alias phase-1 defined (assumed here to be a `std::chrono::system_clock::time_point`-based alias per ARCHITECTURE.md's ISO-8601 discussion) rather than redefining a local type in this phase.
- `ProductionQueueEntry`'s field names (`orderNumber`, `sampleId`, `shortfallQuantity`, `actualProducedQuantity`, `enqueuedAt`, `expectedCompletionAt`) are fixed by phase-4's model; do not rename them here.
- This phase does not implement or test JSON (de)serialization of `ProductionQueueEntry` itself (that's phase-4/phase-3's `ToJson`/`FromJson` and `JsonFileStore` responsibility) — tests here exercise `ProductionService` against the real `ProductionQueueRepository`/`SampleRepository`/`OrderRepository` from phase-5 (backed by real or temp-directory JSON files, per whatever test fixture convention phase-5's own tests established), not against hand-rolled mocks, so that the round-trip through actual persistence is exercised as part of settlement correctness.
- `ProductionService` must not perform any console I/O and must not decide `Confirmed` vs `Producing` at approval time — that branching stays in `OrderService::Approve` (phase-7), which will call `ComputeShortfall`/`Enqueue` only when it has already decided a new order is going to `Producing`.
