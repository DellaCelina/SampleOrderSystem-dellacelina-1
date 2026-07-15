# Phase 8: MonitoringService + ProductionLineViewService

**Depends on:** Phase 5 (repositories), Phase 6 (production-service)
**Touches:** `SampleOrderSystem/Services/MonitoringService.h`, `SampleOrderSystem/Services/MonitoringService.cpp`, `SampleOrderSystem/Services/ProductionLineViewService.h`, `SampleOrderSystem/Services/ProductionLineViewService.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement MonitoringService (calls SettleDueEntries first, then returns per-status order counts excluding Rejected, and per-sample stock with Depleted/InStock label) and ProductionLineViewService (calls SettleDueEntries first, then returns the FIFO queue head as 'in production' plus the remaining tail, each with sample/shortfall/actual quantity/expected completion). Both are read-only query services with no interdependency between each other, and depend only on the repositories (phase-5) and ProductionService's SettleDueEntries (phase-6), not on OrderService (phase-7), so this phase can proceed in parallel with phase-7. Add Services/MonitoringService.h/.cpp and Services/ProductionLineViewService.h/.cpp to SampleOrderSystemTests.vcxproj.

## Detail

## Behavior to implement

This phase adds two read-only query services. Neither depends on the other; both depend only on the repositories (phase-5: `SampleRepository`, `OrderRepository`, `ProductionQueueRepository`) and on `ProductionService::SettleDueEntries(IClock&)` (phase-6). Neither touches `OrderService` (phase-7), so this phase is safe to implement in parallel with phase-7.

Both services are constructed with references to the repositories they need plus an `IClock&`, and both call `ProductionService::SettleDueEntries(clock)` as the very first statement of every public query method, before reading any repository state — mirroring the same settle-then-read rule `OrderService::Approve` uses (Architecture Key Design Decision #3). Neither service does any file I/O directly or writes to any repository itself; all mutation happens inside `SettleDueEntries`.

### `SampleOrderSystem/Services/MonitoringService.h/.cpp`

Header contract:

```cpp
// Services/MonitoringService.h
#pragma once
#include <string>
#include <vector>
#include "../Core/IClock.h"
#include "../Models/Order.h"

class OrderRepository;
class SampleRepository;
class ProductionQueueRepository;

struct OrderStatusCounts {
    int reserved = 0;
    int confirmed = 0;
    int producing = 0;
    int released = 0;
    // Rejected is intentionally NOT a field here — always excluded from monitoring.
};

enum class StockLevel { InStock, Depleted };

struct SampleStockInfo {
    std::string sampleId;
    std::string sampleName;
    int currentStock = 0;
    StockLevel level = StockLevel::InStock; // Depleted iff currentStock == 0
};

class MonitoringService {
public:
    MonitoringService(OrderRepository& orders,
                       SampleRepository& samples,
                       ProductionQueueRepository& queue,
                       IClock& clock);

    // Settles due entries first, then tallies orders by status, excluding Rejected entirely
    // (an order with status Rejected contributes to no field of OrderStatusCounts).
    OrderStatusCounts GetOrderStatusCounts();

    // Settles due entries first, then returns one SampleStockInfo per sample currently
    // registered in SampleRepository, in the repository's natural (load/insertion) order.
    // level is InStock when currentStock > 0, Depleted when currentStock == 0.
    // Negative stock should never occur (repository invariant); if it somehow does, treat
    // it as Depleted rather than throwing — this is a defensive, not load-bearing, branch.
    std::vector<SampleStockInfo> GetSampleStockLevels();

private:
    OrderRepository& orders_;
    SampleRepository& samples_;
    ProductionQueueRepository& queue_;
    IClock& clock_;
};
```

`GetOrderStatusCounts` implementation notes:
- Call `ProductionService::SettleDueEntries(queue_, orders_, samples_, clock_)` (match phase-6's actual signature — see below) first.
- Iterate every order in `OrderRepository`; increment the matching counter for `Reserved/Confirmed/Producing/Released`; do nothing for `Rejected` (no counter field exists for it, so there is nothing to increment — this is the mechanism that enforces "always excluded").
- No finer-grained breakdown (e.g. per-sample counts) is in scope for this phase — the requirement's monitoring screen only asks for a system-wide tally per status, per `docs/REQUIREMENT.md`.

`GetSampleStockLevels` implementation notes:
- Settle first, same as above.
- One entry per sample in `SampleRepository`, using `sample.sampleId`, `sample.name`, `sample.currentStock` directly (this is *raw* current stock, not "unclaimed" stock — the unclaimed-stock formula from Key Design Decision #2 is specific to `OrderService::Approve`'s decision logic, not to what the monitoring screen displays).
- `StockLevel` is a plain binary label per Non-goals ("no finer threshold") — do not add a "low stock" tier.

### `SampleOrderSystem/Services/ProductionLineViewService.h/.cpp`

Header contract:

```cpp
// Services/ProductionLineViewService.h
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "../Core/IClock.h"

class ProductionQueueRepository;
class OrderRepository;
class SampleRepository;

struct ProductionQueueEntryView {
    std::string orderNumber;
    std::string sampleId;
    std::string sampleName;
    int shortfallQuantity = 0;
    int actualProducedQuantity = 0;
    IClock::TimePoint expectedCompletionAt;
};

struct ProductionLineSnapshot {
    // The FIFO head, i.e. the entry currently "in production" — empty when the queue is empty.
    std::optional<ProductionQueueEntryView> inProduction;
    // The remaining FIFO tail, in queue order, NOT including the head above.
    // Empty vector when the queue has 0 or 1 entries.
    std::vector<ProductionQueueEntryView> waiting;
};

class ProductionLineViewService {
public:
    ProductionLineViewService(ProductionQueueRepository& queue,
                                OrderRepository& orders,
                                SampleRepository& samples,
                                IClock& clock);

    // Settles due entries first (removing anything whose expectedCompletionAt has already
    // passed and folding it into stock/order status), then reads whatever remains in the
    // queue file: element 0 becomes `inProduction`, elements 1..N-1 become `waiting`, in
    // the same order the file stores them (array order == FIFO order, per Key Design
    // Decision #6 — no separate position field to sort by).
    ProductionLineSnapshot GetSnapshot();

private:
    ProductionQueueRepository& queue_;
    OrderRepository& orders_;
    SampleRepository& samples_;
    IClock& clock_;
};
```

`GetSnapshot` implementation notes:
- Call `SettleDueEntries` first — this is what guarantees that, by the time this method reads the queue, the "head" it reports as `inProduction` is genuinely not-yet-due (its `expectedCompletionAt > clock.Now()`); an entry that had already matured was removed by the settle step and its order flipped to `Confirmed` before this method ever sees the queue.
- For each remaining `ProductionQueueEntry`, resolve `sampleName` via `SampleRepository::FindById(entry.sampleId)` (the entry itself only stores `sampleId`, per the `ProductionQueueEntry` model in phase-4/phase-5) — if the sample is somehow missing (should not happen given repository invariants), fall back to `sampleId` as the display name rather than throwing, since this is a display service and must not crash on an inconsistency it didn't cause.
- `shortfallQuantity`/`actualProducedQuantity`/`expectedCompletionAt` are copied directly off the `ProductionQueueEntry` — this service does no math of its own; all quantity/time computation is phase-6's `ProductionService`'s responsibility.
- If the queue is empty, `inProduction` is `std::nullopt` and `waiting` is an empty vector — do not synthesize a placeholder entry.

### Signature dependency note (must reconcile with phase-6)

The exact call signature of `SettleDueEntries` is decided by phase-6 (e.g. it may be `ProductionService::SettleDueEntries(IClock&)` as a member with repositories injected into `ProductionService`'s own constructor, or a free/static function taking repositories + clock explicitly). This phase's two services must call it exactly however phase-6 actually defines it — if phase-6 makes `ProductionService` itself the owner of the repository references, then `MonitoringService`/`ProductionLineViewService` should hold a `ProductionService&` (constructed from the same repositories) instead of re-taking repository references and calling a static/free function. Whichever shape phase-6 lands on, do not duplicate the sweep-and-update logic here — always delegate to the one implementation, per Key Design Decision #3. When implementing this phase, check phase-6's actual `ProductionService.h` before writing the constructor signatures above, and adjust the constructor parameter list of both services to match without changing the public query-method signatures (`GetOrderStatusCounts()`, `GetSampleStockLevels()`, `GetSnapshot()`) described above, since those are what downstream Controllers (a later phase) will call.

## Unit tests to write (in `SampleOrderSystemTests`, using `FakeClock`)

Use in-memory/temp-directory repository instances per test (matching whatever fixture pattern phase-5's repository tests already established — e.g. a temp `data/` folder per test case, cleaned up after).

### MonitoringService
1. `GetOrderStatusCounts` returns all-zero counts when `OrderRepository` is empty.
2. `GetOrderStatusCounts` correctly tallies one order in each of `Reserved/Confirmed/Producing/Released` — each increments only its own counter.
3. `GetOrderStatusCounts` excludes `Rejected` orders entirely — a repository containing only `Rejected` orders yields all-zero counts, and a mix of `Rejected` + others does not count the rejected ones anywhere.
4. `GetOrderStatusCounts` settles due entries before counting: seed a `Producing` order with a matching queue entry whose `expectedCompletionAt` is in the past relative to the `FakeClock`; assert the count reflects `Confirmed` (not `Producing`) after calling `GetOrderStatusCounts`, and that calling it again (idempotency) doesn't change the result or double-apply stock.
5. `GetSampleStockLevels` returns one entry per registered sample, correct `sampleId`/`sampleName`/`currentStock`.
6. `GetSampleStockLevels` labels a sample with `currentStock == 0` as `Depleted` and any positive stock as `InStock` — including a boundary case of stock `1`.
7. `GetSampleStockLevels` reflects stock increases from a settlement that ran as part of this same call (seed a past-due queue entry, assert the returned stock already includes the produced quantity).
8. `GetSampleStockLevels` returns raw current stock, not "unclaimed" stock — a sample with an active `Producing`/`Confirmed` claim against it still reports its full `currentStock`, not stock minus the claim (this asserts the distinction from Key Design Decision #2 is not accidentally applied here).

### ProductionLineViewService
9. `GetSnapshot` returns `inProduction == nullopt` and empty `waiting` when the queue is empty.
10. `GetSnapshot` with exactly one queue entry: that entry appears as `inProduction`; `waiting` is empty.
11. `GetSnapshot` with three queue entries: the first (by file/insertion order) is `inProduction`; `waiting` contains the remaining two, in the same relative order they were enqueued (FIFO order preserved, not resorted by completion time or any other field).
12. `GetSnapshot` settles due entries before reading: seed a queue where the head entry's `expectedCompletionAt` is already past on the `FakeClock`; after `GetSnapshot()`, that entry must NOT appear as `inProduction` (it was removed by settlement) — the next entry (if any) becomes the new `inProduction`, or the result is empty if it was the only entry.
13. Each returned `ProductionQueueEntryView` carries through the correct `orderNumber`, `sampleId`, `shortfallQuantity`, `actualProducedQuantity`, and `expectedCompletionAt` unchanged from the underlying `ProductionQueueEntry` — a straight field-copy assertion.
14. `sampleName` is resolved correctly via the sample repository for a normal entry.
15. Defensive case: a queue entry referencing a `sampleId` not present in `SampleRepository` does not throw; `sampleName` falls back to the raw `sampleId` string.
16. Calling `GetSnapshot()` twice in a row with no time advance between calls is idempotent — same result both times, no duplicate settlement side effects (stock not incremented twice).

## Build wiring

Add to `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj` (matching the existing pattern established for other `Services/*` sources per the Architecture's Build/test wiring section — `<ClCompile Include="..\SampleOrderSystem\Services\...">` / `<ClInclude Include="..\SampleOrderSystem\Services\...">`, relative-path items, no new project):
- `..\SampleOrderSystem\Services\MonitoringService.h` / `.cpp`
- `..\SampleOrderSystem\Services\ProductionLineViewService.h` / `.cpp`

These same two `.cpp` files must also already be (or, if this phase lands before `SampleOrderSystem.vcxproj` itself is updated in a later/parallel phase, will need to be) added as `<ClCompile>`/`<ClInclude>` items in `SampleOrderSystem/SampleOrderSystem.vcxproj` for the main app binary to build — confirm both project files list them; this phase's own scope for `.vcxproj` edits is `SampleOrderSystemTests.vcxproj` only per the phase's `touches` list, so if `SampleOrderSystem.vcxproj` also needs the new files added, flag that as a gap against the `touches` list rather than silently editing a file outside this phase's declared scope.

## Notes on scope boundaries for the implementer

- Do not implement any Controller/View code in this phase — that is out of scope (later phase, per Architecture's Console MVC layer section); these two services only need to be unit-tested directly, not through any UI path.
- Do not modify `OrderService`, `ProductionService`, or any repository in this phase — only consume their existing public interfaces from phase-5/phase-6. If a needed accessor is missing from those (e.g. `SampleRepository` lacks an iteration method, or `ProductionQueueRepository` lacks a way to read all remaining entries after settlement), that is a signal phase-5/phase-6 need a small interface addition — surface it rather than reaching into private/file-level state to work around it.
