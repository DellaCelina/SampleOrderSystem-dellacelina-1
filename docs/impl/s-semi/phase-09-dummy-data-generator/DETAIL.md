# Phase 9: DummyDataGenerator

**Depends on:** Phase 4 (domain-models-iso8601), Phase 5 (repositories), Phase 6 (production-service)
**Touches:** `SampleOrderSystem/Services/DummyDataGenerator.h`, `SampleOrderSystem/Services/DummyDataGenerator.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement schema-driven random Sample/Order generation that writes records directly through SampleRepository/OrderRepository/ProductionQueueRepository (not through the OrderService/ProductionService orchestration methods Approve/SettleDueEntries), distributing generated orders across all five statuses and maintaining the same invariants a real order would have. REVISED: to avoid a second, independently-implemented copy of the shortfall/actual-quantity/FIFO-chain-against-tail math (which risks silently diverging from phase-6's rules), DummyDataGenerator must call ProductionService's pure computation functions (ComputeShortfall, ComputeActualQuantity, ComputeCompletionTime — exposed per phase-6) directly to derive each generated Producing order's ProductionQueueEntry (actualProducedQuantity/shortfall/expectedCompletionAt) and matching stock claim, rather than reimplementing that arithmetic. This means phase-9 genuinely depends on phase-6 (not just phase-4/phase-5 as originally claimed), and is NOT independent of it; it can still proceed in parallel with phase-7 and phase-8 (no dependency on OrderService or on MonitoringService/ProductionLineViewService) once phase-6 is done. Add Services/DummyDataGenerator.h/.cpp to SampleOrderSystemTests.vcxproj.

**Namespace/test-framework correction:** everything is in the **global namespace** (no
`SampleOrderSystem::Services`/`Models::`/`Core::`/`Repositories::` wrappers), matching phase-1/2/4's
real committed code. `Order`'s status field type is the free enum `OrderStatus` (not a nested
`Order::Status`). `IClock::Now()` returns `std::chrono::system_clock::time_point` directly. Tests
are GoogleTest, not Catch2 — see phase-1's DETAIL.md superseding note.

## Detail

## Behavior to implement

`DummyDataGenerator` is a pure business-logic class (no console/file I/O of its own beyond what it does through repositories) that populates `data/samples.json`, `data/orders.json`, and `data/production_queue.json` with realistic, invariant-respecting random records, for use by the (later) `DummyDataController`/`--dummy-data` CLI flag. It does **not** call `OrderService::Approve`/`OrderService::SubmitOrder`/`ProductionService::SettleDueEntries` — it writes directly through `SampleRepository`, `OrderRepository`, and `ProductionQueueRepository` (per Key Design Decision in ARCHITECTURE.md's Services section), but it **must** call `ProductionService`'s pure static computation functions to derive the numbers for any generated `Producing` order, rather than re-deriving that math independently. This is the reason this phase depends on phase-6 (`ProductionService`) in addition to phase-4 (repositories) and phase-5 (models), and is why it is listed as parallel-safe only with phase-7/phase-8 (no dependency on `OrderService` or `MonitoringService`/`ProductionLineViewService`).

### Header: `SampleOrderSystem/Services/DummyDataGenerator.h`

```cpp

struct DummyDataOptions {
    int sampleCount = 10;
    int orderCount  = 20;
};

class DummyDataGenerator {
public:
    // seed is mandatory (no default) precisely so tests are always deterministic;
    // a production caller (DummyDataController, a later phase) picks its own seed
    // (e.g. std::random_device{}() for variety, or a fixed constant for a stable demo).
    DummyDataGenerator(SampleRepository& sampleRepo,
                        OrderRepository& orderRepo,
                        ProductionQueueRepository& queueRepo,
                        IClock& clock,
                        unsigned int seed);

    // Generates `count` new Sample records with randomized-but-valid attributes and
    // persists them via SampleRepository::Add. Returns the generated sample IDs, in
    // generation order. count <= 0 is a no-op returning {}.
    std::vector<std::string> GenerateSamples(int count);

    // Generates `count` new Order records, each targeting a sample drawn (uniformly,
    // via the injected RNG) from `sampleIds`, distributed across all five statuses
    // (Reserved/Confirmed/Producing/Released/Rejected) as described below, maintaining
    // the same stock/queue invariants a real order of that status would have. Returns
    // the generated order numbers, in generation order.
    // Throws std::invalid_argument if sampleIds is empty and count > 0.
    // count <= 0 is a no-op returning {}.
    std::vector<std::string> GenerateOrders(int count, const std::vector<std::string>& sampleIds);

    // Convenience wrapper: GenerateSamples(options.sampleCount) then
    // GenerateOrders(options.orderCount, <that result>). Intended for the CLI/menu entry
    // point; not required by any other phase's contract.
    void GenerateAll(const DummyDataOptions& options = {});

private:
    OrderStatus PickStatusForSlot(int slotIndex, int totalCount) const;
    int  UnclaimedStock(const Sample& sample) const;              // currentStock - runningClaims_[sampleId]
    void RecordClaim(const std::string& sampleId, int quantity);         // runningClaims_[sampleId] += quantity
    void TopUpStockIfNeeded(Sample& sample, int minUnclaimed);   // raises currentStock, persists via SampleRepository
    std::string NextSampleId();       // "SMP-####", derived from max existing suffix + 1 (see below)
    std::string RandomCustomerName();
    std::string RandomSampleName();
    int    RandomQuantity(int lo, int hi);

    SampleRepository&         sampleRepo_;
    OrderRepository&          orderRepo_;
    ProductionQueueRepository& queueRepo_;
    IClock&                           clock_;
    std::mt19937                            rng_;
    std::unordered_map<std::string,int>     runningClaims_; // per-sample cumulative Producing+Confirmed claim,
                                                              // seeded at construction from pre-existing orders
};
```

Note on repository method names: the exact method names (`SampleRepository::Add`, `OrderRepository::Add`/`NextOrderNumber`, `ProductionQueueRepository::Append`/`GetAll`) must match whatever phase-4/phase-5 actually expose — the names above are illustrative. What is normative and must not drift is the *behavior* described in this document (what gets claimed, when stock changes, how completion time is derived).

## Concrete generation rules

### `GenerateSamples(count)`
For each of `count` samples:
- `sampleId`: derived the same way `OrderRepository::NextOrderNumber` derives order numbers (ARCHITECTURE.md Key Design Decision #8) — scan `SampleRepository`'s existing IDs matching a generator-owned pattern `SMP-####`, take `max(existing suffix) + 1`, and increment per generated sample. This guarantees no collision with pre-existing samples without needing retry-on-collision logic. (If phase-4's `SampleRepository`/schema constrains `sampleId` to a different pattern, adapt the prefix — the "derive from max + 1" strategy is the substantive point, not the literal string `SMP-`.)
- `name`: built from a small fixed word pool (e.g. combining a material/component token with a variant letter) plus the numeric suffix, so it's readable and non-colliding without needing uniqueness (name is not required unique).
- `averageProductionTimeMinutes`: random int, `[1, 120]`.
- `yield`: random double in `(0, 1]`, drawn e.g. as `uniform(50, 100) / 100.0` so it's never `0` (schema requires `0 < yield <= 1`).
- `currentStock`: random int in `[0, 500]`.
- Persist via `SampleRepository::Add`.

Returns the list of generated sample IDs, in order.

### `GenerateOrders(count, sampleIds)`
Guard: if `sampleIds` is empty and `count > 0`, throw `std::invalid_argument`. If `count <= 0`, return `{}` without touching any repository.

**Status assignment.** Build a round-robin sequence over the five statuses `[Reserved, Confirmed, Producing, Released, Rejected]`; slot `i`'s status is `sequence[i % 5]`, then the *order of assignment across generated orders* (not the status set itself) is shuffled using `rng_` so runs don't always look mechanically repetitive. This guarantees: when `count >= 5`, all five statuses appear at least once (round-robin coverage is exact, only the interleaving order is randomized). When `count < 5`, only the first `count` statuses in sequence order appear — documented as a known, accepted limitation (coverage of all five is only guaranteed at `count >= 5`), not a bug.

For each slot, in the (shuffled) assignment order:
1. Pick a sample uniformly from `sampleIds` via `rng_`. Look it up via `SampleRepository::FindById`.
2. Pick `customerName` from a small fixed name pool.
3. Compute `unclaimed = UnclaimedStock(sample)` = `max(0, sample.currentStock - runningClaims_[sample.sampleId])` — the exact same formula as `OrderService::Approve` (ARCHITECTURE.md Key Design Decision #2), applied here directly rather than through `OrderService` since this generator bypasses that service by design.
4. Branch on the assigned status:

   - **Reserved**: `quantity = RandomQuantity(1, 200)` (unconstrained — a real Reserved order hasn't been evaluated against stock yet either). Create the order with that status. No stock/queue effect, no `runningClaims_` update.

   - **Rejected**: same as Reserved (any positive quantity is valid — rejection happens before stock matters). Create the order with status `Rejected`. No stock/queue effect, no `runningClaims_` update.

   - **Confirmed**: needs `quantity <= unclaimed`. If `unclaimed < 1`, call `TopUpStockIfNeeded(sample, minUnclaimed)` first — pick a top-up target (`unclaimed + RandomQuantity(1, 200)`), increase `sample.currentStock` by that amount, and persist the updated sample via `SampleRepository` (its update/mutation method, per phase-5), then recompute `unclaimed`. Then pick `quantity` uniformly in `[1, unclaimed]`. Create the order with status `Confirmed`. Call `RecordClaim(sampleId, quantity)` (adds to `runningClaims_`) — Confirmed orders count toward unclaimed-stock the same as real ones. No queue entry; no stock decrement (decrementing only happens at Release, per ARCHITECTURE.md's Release data flow).

   - **Producing**: must have a genuine shortfall (never allowed to be a Producing order whose shortfall would compute to 0 — that would misrepresent the status). Pick `quantity` uniformly in `[unclaimed + 1, unclaimed + 200]`, guaranteeing `quantity > unclaimed`. Then:
     - `shortfall = ProductionService::ComputeShortfall(quantity, unclaimed)`
     - `actualProducedQuantity = ProductionService::ComputeActualQuantity(shortfall, sample.yield)`
     - Determine the FIFO tail: read the current last entry (if any) from `ProductionQueueRepository`'s full contents (across *all* samples — the production line is one global FIFO, not per-sample, per ARCHITECTURE.md's Components section on `ProductionService`); use its `expectedCompletionAt` as the anchor, or `clock_.Now()` if the queue is empty.
     - `enqueuedAt = clock_.Now()`
     - `auto durationMinutes = ProductionService::ComputeProductionDurationMinutes(actualProducedQuantity, sample.averageProductionTimeMinutes);` (matches phase-6's `ComputeProductionDurationMinutes` signature exactly — do not inline the multiplication here)
     - `auto previousTailCompletion = <the anchor read above, as std::optional<std::chrono::system_clock::time_point> — nullopt if the queue was empty>;`
     - `expectedCompletionAt = ProductionService::ComputeCompletionTime(enqueuedAt, previousTailCompletion, durationMinutes);` — this matches phase-6's real 3-arg contract (`std::chrono::system_clock::time_point enqueuedAt, std::optional<std::chrono::system_clock::time_point> previousTailCompletion, int durationMinutes`) exactly; the anchor/tail value is passed as the *second* argument (`previousTailCompletion`), never as the first (`enqueuedAt` is always `clock_.Now()`), since swapping them would invert the FIFO `max(enqueuedAt, previousTailCompletion)` semantics.
     - Persist a new `ProductionQueueEntry{orderNumber, sampleId, shortfall, actualProducedQuantity, enqueuedAt, expectedCompletionAt}` via `ProductionQueueRepository`, appended (so it becomes the new tail for the *next* generated Producing entry in this same call — entries must chain correctly against each other, not just against pre-existing queue contents).
     - Create the order with status `Producing`.
     - Call `RecordClaim(sampleId, quantity)` — full requested quantity, matching Key Design Decision #2 (Producing claims the full quantity, not just the shortfall).
     - No stock mutation on `Sample.currentStock` itself (Producing doesn't change stock; only settlement, which this generator deliberately bypasses, does that).

   - **Released**: needs `quantity <= unclaimed` (same feasibility check/top-up as Confirmed — reuse `TopUpStockIfNeeded`). Pick `quantity` uniformly in `[1, unclaimed]`. Create the order directly with status `Released` (simulating an order that already went `Reserved → Confirmed → Released`). Then **decrement** `sample.currentStock` by `quantity` and persist, mirroring `OrderService::Release`'s effect. Do **not** call `RecordClaim` for this order — a Released order is excluded from the unclaimed-stock claim sum (only Producing/Confirmed count, per Key Design Decision #2), and its effect is already reflected by the stock decrement itself.

5. Persist the order via `OrderRepository::Add` (real order-number allocation, e.g. `OrderRepository::NextOrderNumber()`) with the status set directly at creation (not via a state-transition method, since this generator is explicitly not routing through `OrderService`).

`runningClaims_` must be seeded at `DummyDataGenerator` construction time by scanning `OrderRepository`'s existing orders and summing quantities of pre-existing `Producing`/`Confirmed` orders per `sampleId` — otherwise a dummy-generation run on top of already-populated data would under-count claims and could generate a Confirmed/Producing order that double-claims stock a real order already claimed.

## Edge cases / invariants this phase must guarantee
- Never generate a `Producing` order whose computed `shortfall` is 0 (would misrepresent why it's Producing).
- Never generate a `Confirmed` or `Released` order whose `quantity` exceeds unclaimed stock at the moment of generation (top-up rather than violate).
- `Released` orders decrement stock and are excluded from `runningClaims_`; `Producing`/`Confirmed` orders are included and do not touch stock directly.
- FIFO chaining for `Producing` entries generated within one `GenerateOrders` call must be correct relative to each other (each new entry's anchor is the true current tail, including entries this same call just appended), not just relative to whatever was already in the queue file before the call.
- All numeric fields respect schema constraints (`quantity > 0`, `yield` in `(0,1]`, `averageProductionTimeMinutes > 0`, non-negative stock).
- Sample/order IDs never collide with pre-existing records.
- `count <= 0` is a documented no-op (not an error) for both `GenerateSamples` and `GenerateOrders`; an empty `sampleIds` with `count > 0` is a documented error (`std::invalid_argument`).
- Coverage of all five statuses is only *guaranteed* when `orderCount >= 5`; below that, coverage is partial by design — call this out in a test rather than silently expecting full coverage at small counts.
- Determinism: same seed + same starting repository state must produce identical generated records (needed for reproducible bug reports/test fixtures) — note that this determinism is scoped to a single toolchain (MSVC's STL `std::mt19937`/`std::uniform_int_distribution`), not cross-platform-stable, which is fine since the whole project only ever builds with MSVC.

## Unit tests to write (GoogleTest/GoogleMock, using a `FakeClock` and real repository instances pointed at a fresh temp directory per test — no mocks of the repositories themselves, since the point is to verify actual persisted JSON state)

1. `GenerateSamples_CreatesRequestedCount_WithValidFields` — generate N samples; assert N records land in `SampleRepository`, each with unique `sampleId`, `averageProductionTimeMinutes > 0`, `0 < yield <= 1`, `currentStock >= 0`.
2. `GenerateSamples_ZeroOrNegativeCount_IsNoOp` — `count = 0` and `count = -1` both return `{}` and leave the repository untouched.
3. `GenerateSamples_NeverCollidesWithPreexistingSampleIds` — pre-seed the repo with a sample using the same ID pattern the generator would pick next; assert the generator still produces a non-colliding ID (derived from max existing suffix + 1).
4. `GenerateOrders_EmptySampleIds_Throws` — `GenerateOrders(3, {})` throws `std::invalid_argument`.
5. `GenerateOrders_ZeroOrNegativeCount_IsNoOp` — returns `{}`, no repository writes, even with a non-empty `sampleIds`.
6. `GenerateOrders_CreatesRequestedCount_WithValidOrderNumbers` — order numbers are well-formed `ORD-####`, unique, and continue correctly from any pre-existing max in `OrderRepository`.
7. `GenerateOrders_AllFiveStatusesAppear_WhenCountAtLeastFive` — generate >=5 (e.g. 25) orders across several samples; assert all of `Reserved/Confirmed/Producing/Released/Rejected` appear at least once.
8. `GenerateOrders_FewerThanFiveOrders_StillProducesValidRecords` — `count = 3`; no crash, and whichever statuses were produced still individually satisfy their own invariants (test 9–14 below, just on a smaller/partial set).
9. `GenerateOrders_ProducingOrder_ShortfallIsAlwaysPositive` — for every generated `Producing` order, recompute `unclaimed` at its generation point and assert `quantity > unclaimed` (i.e. shortfall via `ProductionService::ComputeShortfall` is > 0).
10. `GenerateOrders_ProducingOrder_QueueEntryMatchesProductionServiceMath` — for a generated Producing order, assert the persisted `ProductionQueueEntry` has `shortfallQuantity == ProductionService::ComputeShortfall(...)`, `actualProducedQuantity == ProductionService::ComputeActualQuantity(shortfall, sample.yield)`, computed independently in the test using the *same* `ProductionService` functions (not a re-derived formula) and a controlled `FakeClock`.
11. `GenerateOrders_ProducingEntries_ChainFifoCorrectlyAcrossMultipleGeneratedEntries` — generate several Producing orders in one call (e.g. force via seed/enough count), and assert each subsequent entry's `expectedCompletionAt` is derived (via `ProductionService::ComputeCompletionTime`) from the *previous generated entry's* `expectedCompletionAt`, not from a stale/empty anchor — verifying the tail-tracking logic within a single call.
12. `GenerateOrders_ProducingOrder_DoesNotMutateSampleStockDirectly` — a sample's `currentStock` is unchanged immediately after a Producing order is generated for it (only the running-claims accounting changes).
13. `GenerateOrders_ConfirmedOrder_QuantityNeverExceedsUnclaimedStockAtGenerationTime` — replay the generation sequence and assert every Confirmed order's quantity was `<=` unclaimed stock computed from `(sample.currentStock at that point) - (sum of other Producing/Confirmed quantities claimed so far)`.
14. `GenerateOrders_ConfirmedOrder_TopsUpStockWhenUnclaimedIsZero` — construct a scenario (small pre-seeded `currentStock`, drive via a fixed seed/enough prior claims) where unclaimed hits 0 before a Confirmed order is generated; assert the sample's `currentStock` was increased (top-up occurred) and the resulting order is a valid Confirmed order whose quantity fits the new unclaimed stock.
15. `GenerateOrders_ReleasedOrder_DecrementsSampleStockByQuantity` — for a generated Released order, assert `sample.currentStock` after generation is exactly `(currentStock before) - quantity`, and that this order's quantity is *not* included in the running-claims accounting used by later Confirmed/Producing generations in the same run.
16. `GenerateOrders_RejectedAndReservedOrders_HaveNoStockOrQueueEffect` — for generated Reserved/Rejected orders, assert no `ProductionQueueEntry` references their order number and no sample's stock changed as a direct result of generating them.
17. `GenerateOrders_SeedsRunningClaimsFromPreexistingOrders` — pre-populate `OrderRepository` with a real `Producing` order for a sample (with a plausible pre-existing quantity) before constructing `DummyDataGenerator`; then generate a Confirmed order for that same sample and assert its quantity respects the pre-existing order's claim (i.e., unclaimed stock accounts for it), proving the constructor-time seeding of `runningClaims_` works, not just claims recorded during this run.
18. `GenerateOrders_DeterministicWithFixedSeed` — two separate `DummyDataGenerator` instances constructed with the same seed against identically-initialized fresh repositories produce byte-identical generated records (same IDs, quantities, statuses, timestamps).
19. `GenerateSamples_And_GenerateOrders_QuantityAndNumericFieldsRespectSchemaBounds` — sweep all generated orders/samples and assert `quantity > 0`, `yield` bounds, `averageProductionTimeMinutes > 0`, non-negative `currentStock`, for every record produced across a reasonably large generation run (e.g. 50 samples / 100 orders) to catch any boundary slip in the random-range helpers.
20. `GenerateAll_WiresSamplesIntoOrders` — `GenerateAll({sampleCount: 5, orderCount: 10})` results in every generated order's `sampleId` referencing one of the 5 generated samples (not some other pre-existing or invalid sample id), verifying the convenience wrapper correctly threads `GenerateSamples`'s output into `GenerateOrders`.

## Build wiring
Add `Services/DummyDataGenerator.h` and `Services/DummyDataGenerator.cpp` as `<ClInclude>`/`<ClCompile>` items to `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`, alongside the other non-UI sources it already compiles directly (per ARCHITECTURE.md's Build/test wiring section — each `.vcxproj` compiles the shared sources into its own independent binary; no linker conflict). Flag for the implementer: this phase's declared `touches` only lists `SampleOrderSystemTests.vcxproj`, not `SampleOrderSystem.vcxproj` — but `DummyDataGenerator` is application source that a later `DummyDataController`/CLI-flag phase will need compiled into the real `SampleOrderSystem.exe` too. Whichever phase adds `DummyDataController`/the CLI wiring should double-check `Services/DummyDataGenerator.*` is also added to `SampleOrderSystem.vcxproj`'s item groups at that point, since this phase does not do so.

## Interfaces this phase exposes to later phases
- `DummyDataGenerator` (constructor + `GenerateSamples`/`GenerateOrders`/`GenerateAll` as specified above) is the integration point a later `DummyDataController` phase will call directly — no other phase in the current plan declares a dependency on phase-9, but this signature should be treated as the stable contract for that future wiring.
