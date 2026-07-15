# Phase 5: Repositories (Sample/Order/ProductionQueue) + order-number sequence derivation

**Depends on:** Phase 3 (schema-persistence), Phase 4 (domain-models-iso8601)
**Touches:** `SampleOrderSystem/Repositories/SampleRepository.h`, `SampleOrderSystem/Repositories/SampleRepository.cpp`, `SampleOrderSystem/Repositories/OrderRepository.h`, `SampleOrderSystem/Repositories/OrderRepository.cpp`, `SampleOrderSystem/Repositories/ProductionQueueRepository.h`, `SampleOrderSystem/Repositories/ProductionQueueRepository.cpp`, `data/samples.json`, `data/orders.json`, `data/production_queue.json`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement the three repositories wrapping JsonFileStore for their respective tables: SampleRepository (CRUD, FindById exact match, FindByNameSubstring case-insensitive, stock mutation helpers), OrderRepository (append-only creation, status-transition updates, and the restart-safe NextOrderNumber() that re-derives the sequence counter as max(existing ORD-#### suffixes)+1 from data/orders.json at load time), and ProductionQueueRepository (append/iterate/remove preserving file-array order as FIFO order, no separate sequence field). Depends on both the persistence layer (phase-3) and the domain models (phase-4); this is the first phase where the two combine. Add Repositories/*.h/.cpp to SampleOrderSystemTests.vcxproj alongside this phase's tests.

## Detail

## Preconditions this phase assumes (from phase-3 / phase-4)

- `SampleOrderSystem/Persistence/JsonFileStore.h` exposes (approximately):
  ```cpp
  template <typename T>
  class JsonFileStore {
  public:
      // path: e.g. "data/samples.json"; schemaPath: e.g. "schema/sample.schema.json"
      JsonFileStore(std::filesystem::path dataPath, std::filesystem::path schemaPath,
                    std::function<Json::JsonValue(const T&)> toJson,
                    std::function<T(const Json::JsonValue&)> fromJson);

      // Throws JsonFileStoreException (or similar) on parse failure or schema-validation
      // failure of ANY record; never returns a partial table. If the file does not exist,
      // returns an empty vector<T> (this is a *new* table, not a corrupt one).
      std::vector<T> Load() const;

      // Serializes the whole vector, writes to a temp file, atomically renames over dataPath.
      void SaveAll(const std::vector<T>& records) const;
  };
  ```
  If phase-3 actually shipped a different shape (e.g. non-template, `JsonValue`-only interface
  with repositories doing their own `T`↔`JsonValue` mapping via each model's `ToJson`/`FromJson`),
  repositories in this phase adapt to whatever the real signature is — the important contract this
  phase depends on either way is: **load is whole-table fail-fast** (one bad record fails the
  whole load, no silent partial application) and **save is atomic (temp+rename)**. If phase-3's
  actual API differs enough that this phase can't cleanly adapt, that's a real cross-phase
  interface gap — flag it back rather than silently reshaping the store to match this guess.
- `SampleOrderSystem/Models/Sample.h`: struct/class with `sampleId` (string), `name` (string),
  `averageProductionTimeMinutes` (int), `yield` (double), `currentStock` (int); free functions or
  static methods `SampleToJson(const Sample&) -> JsonValue` / `SampleFromJson(const JsonValue&) -> Sample`.
- `SampleOrderSystem/Models/Order.h`: `orderNumber` (string, `ORD-####`), `sampleId` (string),
  `customerName` (string), `quantity` (int), `status` (enum `OrderStatus { Reserved, Confirmed,
  Producing, Released, Rejected }`); `OrderToJson`/`OrderFromJson`.
- `SampleOrderSystem/Models/ProductionQueueEntry.h`: `orderNumber`, `sampleId`, `shortfallQuantity`
  (int), `actualProducedQuantity` (int), `enqueuedAt` (`IClock::TimePoint`), `expectedCompletionAt`
  (`IClock::TimePoint`); `ProductionQueueEntryToJson`/`FromJson` (which internally use the shared
  `TimePointToIso8601`/`ParseIso8601` free functions per ARCHITECTURE.md).

This phase does not modify any of the above — it only consumes them. If any of these
signatures don't actually exist yet when this phase starts, that is a blocking dependency
gap on phase-3/phase-4, not something to route around by inventing a parallel model.

## Files to add

- `SampleOrderSystem/Repositories/SampleRepository.h/.cpp`
- `SampleOrderSystem/Repositories/OrderRepository.h/.cpp`
- `SampleOrderSystem/Repositories/ProductionQueueRepository.h/.cpp`
- Update `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`: add `<ClInclude>`/`<ClCompile>`
  items for all six new files (relative path into `..\SampleOrderSystem\Repositories\`), matching
  the pattern already used for `Core/`, `Json/`, `Persistence/`, `Models/` from earlier phases.
- Corresponding test files (new, added to the same vcxproj): `SampleOrderSystemTests/SampleRepositoryTests.cpp`,
  `OrderRepositoryTests.cpp`, `ProductionQueueRepositoryTests.cpp`.
- `data/samples.json`, `data/orders.json`, `data/production_queue.json` — this phase is
  responsible for making sure these files can be absent (fresh install) or empty-array (`[]`) and
  still load cleanly as an empty in-memory table; it does not need to seed them with real data
  (that's dummy-data-generator territory, a later phase). Tests should use temp-directory copies
  of these paths (or a `JsonFileStore` constructed against a scratch path under the test binary's
  working directory / a `std::filesystem::temp_directory_path()` subfolder), never the real
  `data/*.json` the running app would use, so test runs never corrupt or depend on shared state.

## SampleRepository

```cpp
// SampleOrderSystem/Repositories/SampleRepository.h
class SampleRepository {
public:
    explicit SampleRepository(std::filesystem::path dataPath = "data/samples.json",
                               std::filesystem::path schemaPath = "schema/sample.schema.json");

    // Loads all records from disk into an in-memory cache at construction time (fail-fast per
    // JsonFileStore contract: throws/propagates on malformed file). All subsequent methods
    // operate on the in-memory cache and persist via SaveAll after each mutation (no incremental
    // JSON patching — the whole table is small enough this is fine, matches JsonFileStore's
    // whole-table load/save shape).

    // Returns false (no mutation, no exception) if sampleId already exists — "rejects duplicate
    // sampleId without mutating the existing record" per REQUIREMENT.md. Returns true on success.
    bool Add(const Sample& sample);

    std::vector<Sample> FindAll() const;

    // Exact-match on sampleId. Returns std::nullopt if not found.
    std::optional<Sample> FindById(const std::string& sampleId) const;

    // Case-insensitive substring match on name. Returns empty vector if none match.
    // Empty needle matches every sample (substring of everything) -- explicit test for this.
    std::vector<Sample> FindByNameSubstring(const std::string& needle) const;

    // Stock mutation helpers used by ProductionService/OrderService in later phases.
    // Both throw std::invalid_argument (or return false — pick one, see Decision below) if
    // sampleId doesn't exist. Decision for this phase: THROW std::invalid_argument, since callers
    // (services) are expected to have already validated existence via FindById and a missing
    // sample at this point is a programming error, not a recoverable user input error.
    void IncreaseStock(const std::string& sampleId, int amount); // amount must be > 0 (assert/throw if <= 0)
    void DecreaseStock(const std::string& sampleId, int amount); // amount must be > 0; throws
                                                                   // std::invalid_argument if it
                                                                   // would take stock negative
                                                                   // (defensive; callers should
                                                                   // never do this given the
                                                                   // unclaimed-stock formula, but
                                                                   // the repository itself must
                                                                   // not silently allow negative
                                                                   // stock into the JSON file)

private:
    std::filesystem::path dataPath_;
    JsonFileStore<Sample> store_; // or however phase-3 actually shapes this
    std::vector<Sample> cache_;
    void Persist(); // calls store_.SaveAll(cache_)
};
```

Notes/edge cases to cover in tests (`SampleRepositoryTests.cpp`):
- Construct against an empty/non-existent file path → `FindAll()` returns `{}`, no throw.
- `Add` a sample, then `FindById` returns it with all fields intact (round-trip through the cache,
  not yet re-reading from disk).
- `Add` a duplicate `sampleId` (same or different other fields) → returns `false`; `FindById`
  still returns the *original* record unchanged (verify by field, not just non-null).
- `FindById` on unknown id → `std::nullopt`.
- `FindByNameSubstring("gaas")` matches a sample named `"GaAs Wafer"` (case difference) and does
  not match an unrelated name; matches multiple samples when more than one qualifies; returns `{}`
  when none qualify.
- `IncreaseStock`/`DecreaseStock` on an existing sample updates `currentStock` correctly (verify
  via `FindById` after the call, not only via an internal counter).
- `IncreaseStock`/`DecreaseStock` on unknown `sampleId` throws `std::invalid_argument`.
- `DecreaseStock` past zero throws `std::invalid_argument` and leaves stock unchanged (assert
  post-throw `currentStock` still equals pre-call value, i.e. the operation is all-or-nothing, not
  partially applied then thrown).
- **Persistence round-trip**: construct a `SampleRepository` against a real temp file path, `Add`
  two samples, mutate stock on one, then construct a **second, independent** `SampleRepository`
  instance against the same path and assert `FindAll()` on the second instance matches the first
  instance's in-memory state exactly (field-by-field) — this is the test that actually exercises
  `JsonFileStore` save/load through this repository, distinct from the in-memory-cache-only tests
  above.
- A malformed `data/samples.json` (e.g. missing required field, or `yield` outside `(0, 1]` if
  schema validation from phase-3 enforces it) causes the `SampleRepository` constructor to
  propagate the load failure (test asserts a throw, not a silently-empty repository) — this
  behavior actually lives in `JsonFileStore`/`SchemaValidator` from phase-3, but this phase's test
  suite should include at least one smoke test confirming the repository doesn't swallow that
  exception or catch-and-ignore it.

## OrderRepository

```cpp
// SampleOrderSystem/Repositories/OrderRepository.h
class OrderRepository {
public:
    explicit OrderRepository(std::filesystem::path dataPath = "data/orders.json",
                              std::filesystem::path schemaPath = "schema/order.schema.json");
    // Constructor Load()s all orders, then derives the sequence counter:
    // nextSequence_ = 1 + max over all loaded orders of ParseOrdNumberSuffix(order.orderNumber),
    // or 1 if the table is empty. ParseOrdNumberSuffix extracts the 4-digit numeric suffix from
    // "ORD-####" (e.g. "ORD-0007" -> 7). This derivation happens ONCE, at construction; it is not
    // recomputed lazily on every NextOrderNumber() call.

    // Returns "ORD-####" (4-digit, zero-padded) and increments the in-memory counter. Does NOT
    // create/persist an order by itself -- callers (OrderService, a later phase) call this to
    // obtain a number, then call Add() with the fully-formed Order. Rollover past 9999 is
    // explicitly out of scope (per REQUIREMENT.md) -- do not add wraparound/expansion logic;
    // if nextSequence_ would exceed 9999, formatting behavior is unspecified/untested this phase
    // (leave it as whatever %04d does -- 5 digits -- rather than guessing a policy).
    std::string NextOrderNumber();

    // Appends a new order (expected status Reserved, but this method does not itself enforce
    // that -- validation of "new orders start Reserved" belongs to OrderService, not the
    // repository). Throws std::invalid_argument if orderNumber already exists in the table
    // (defensive -- should never happen if callers always go through NextOrderNumber first, but
    // the repository must not silently create a duplicate).
    void Add(const Order& order);

    std::vector<Order> FindAll() const;
    std::optional<Order> FindByOrderNumber(const std::string& orderNumber) const;

    // Returns all orders with status == Reserved.
    std::vector<Order> FindByStatus(OrderStatus status) const;

    // Returns all orders whose sampleId matches, in any status. Required by phase-7's
    // OrderService::ComputeUnclaimedStock (sums quantity across a sample's Producing/Confirmed
    // orders) -- exposed here as a repository-level filter rather than making phase-7 filter
    // FindAll() client-side, so the "query by sample" access pattern has one implementation.
    std::vector<Order> FindBySampleId(const std::string& sampleId) const;

    // Updates the status field of an existing order in place and persists. Throws
    // std::invalid_argument if orderNumber not found. Does NOT validate that the transition is
    // legal (Reserved->Confirmed etc.) -- that state-machine validation belongs to OrderService in
    // a later phase; this repository method is a dumb "set status and save" primitive. (This is a
    // deliberate scope line: repositories don't know business rules, only storage shape.)
    void UpdateStatus(const std::string& orderNumber, OrderStatus newStatus);

private:
    std::filesystem::path dataPath_;
    JsonFileStore<Order> store_;
    std::vector<Order> cache_;
    int nextSequence_;
    void Persist();
    static int ParseOrdNumberSuffix(const std::string& orderNumber); // throws std::invalid_argument
                                                                       // if it doesn't match ORD-####
};
```

Notes/edge cases to cover in tests (`OrderRepositoryTests.cpp`):
- Construct against empty/non-existent file → `NextOrderNumber()` returns `"ORD-0001"`.
- Construct against a file pre-seeded (via a first repository instance, or by writing JSON
  directly in the test) with orders `ORD-0001`, `ORD-0002`, `ORD-0003` → a **new** `OrderRepository`
  instance constructed against that same path returns `"ORD-0004"` from its first
  `NextOrderNumber()` call — this is the core "restart-safe sequence derivation" behavior and must
  be tested via two independent repository instances against the same file, not just one
  instance's in-memory counter.
- Gaps in the sequence are tolerated: pre-seed with `ORD-0001`, `ORD-0005` only (simulating some
  other non-monotonic history, e.g. hand-edited test fixture) → next repository instance derives
  `ORD-0006` (max+1, not count+1) — explicit test distinguishing "max of suffixes" from "count of
  records" since these differ once there are gaps.
- Two calls to `NextOrderNumber()` on the same instance without an intervening `Add` still return
  distinct increasing numbers (`ORD-0001`, then `ORD-0002`) — confirms the counter advances on
  call, not on successful `Add`.
- `Add` with a duplicate `orderNumber` throws `std::invalid_argument`; table unchanged after
  (verify via `FindAll().size()` before/after the throwing call).
- `FindBySampleId` returns only orders matching the given `sampleId`, across all statuses
  (including `Reserved`/`Rejected`, not just `Producing`/`Confirmed`); returns an empty vector for
  a `sampleId` with no orders, without throwing.
- `FindByOrderNumber` unknown → `std::nullopt`.
- `FindByStatus(Reserved)` returns only Reserved orders when the table has a mix of statuses
  (seed at least one of each of the 5 statuses in one test and assert exact membership per status
  filter, including `Rejected` orders being returned by `FindByStatus(Rejected)` itself but never
  leaking into `FindByStatus(Reserved)` etc. -- filtering correctness, not exclusion policy, which
  is OrderService's job in a later phase).
- `UpdateStatus` on a known order changes only that order's status field, leaves all its other
  fields (`sampleId`, `customerName`, `quantity`, `orderNumber`) untouched, and leaves all other
  orders in the table untouched — verify via `FindAll()` diff before/after.
- `UpdateStatus` on unknown `orderNumber` throws `std::invalid_argument`; table unchanged.
- **Persistence round-trip**: `Add` several orders with varying statuses across one repository
  instance, `UpdateStatus` on one, then construct a second independent instance against the same
  path and assert `FindAll()` matches exactly (including that the derived `nextSequence_` on the
  second instance, exposed indirectly via `NextOrderNumber()`, continues from the right max).

## ProductionQueueRepository

```cpp
// SampleOrderSystem/Repositories/ProductionQueueRepository.h
class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(
        std::filesystem::path dataPath = "data/production_queue.json",
        std::filesystem::path schemaPath = "schema/production_queue.schema.json");

    // Appends to the end of the in-memory array and persists. No sequence/position field is
    // added to the entry -- array order IS FIFO order (per ARCHITECTURE.md Key Design Decision
    // #6). This repository does not enforce FIFO completion-time ordering invariants (e.g. that
    // a later entry's expectedCompletionAt >= an earlier entry's) -- that's ProductionService's
    // job (a later phase) at enqueue-time; this method just appends whatever entry it's given.
    void Enqueue(const ProductionQueueEntry& entry);

    // Returns all entries in file/array (== FIFO) order, head first.
    std::vector<ProductionQueueEntry> FindAllInOrder() const;

    // Returns the first (FIFO head) entry, or std::nullopt if the queue is empty. This is what
    // ProductionLineViewService (later phase) uses for "currently in production".
    std::optional<ProductionQueueEntry> PeekHead() const;

    // Removes the entry matching orderNumber, preserving the relative order of all remaining
    // entries (i.e. this is an erase-by-key from the middle/front of a vector, not a
    // pop-and-reshuffle) and persists. Throws std::invalid_argument if orderNumber not found in
    // the queue (defensive -- callers, i.e. ProductionService's settlement sweep in a later
    // phase, should only ever remove entries they just found via FindAllInOrder).
    void Remove(const std::string& orderNumber);

private:
    std::filesystem::path dataPath_;
    JsonFileStore<ProductionQueueEntry> store_;
    std::vector<ProductionQueueEntry> cache_;
    void Persist();
};
```

Notes/edge cases to cover in tests (`ProductionQueueRepositoryTests.cpp`):
- Construct against empty/non-existent file → `FindAllInOrder()` returns `{}`, `PeekHead()`
  returns `std::nullopt`.
- `Enqueue` three entries (A, B, C) in that order → `FindAllInOrder()` returns exactly
  `[A, B, C]` in that order (assert the sequence, not just membership) → `PeekHead()` returns `A`.
- `Remove("A's orderNumber")` from a three-entry queue `[A, B, C]` → `FindAllInOrder()` becomes
  `[B, C]` (order preserved, not e.g. `[C, B]` from a swap-and-pop) → `PeekHead()` now returns `B`.
- `Remove` a middle entry (`[A, B, C]` remove B) → `FindAllInOrder()` becomes `[A, C]`, order
  preserved.
- `Remove` an unknown `orderNumber` throws `std::invalid_argument`; queue unchanged (verify
  `FindAllInOrder()` before/after).
- **Persistence round-trip**: `Enqueue` two entries with distinct `enqueuedAt`/`expectedCompletionAt`
  timestamps (use fixed `IClock::TimePoint` values constructed directly, not a real clock, since
  this phase has no clock dependency of its own -- it just stores whatever `TimePoint` it's given)
  on one instance, then a second independent instance against the same path returns the same two
  entries in the same order with timestamps equal to the originals (this specifically exercises
  the ISO-8601 `TimePoint` round-trip through `ProductionQueueEntry::ToJson`/`FromJson`, which is
  phase-4's responsibility to have gotten right, but this repository's round-trip test is what
  would catch a regression in that path from this phase's perspective too).

## Cross-cutting design notes (apply to all three repositories)

1. **Load-once, cache, mutate-in-memory-then-full-resave** is the consistent pattern across all
   three repositories in this phase, matching how `JsonFileStore` is described as a whole-table
   load/save abstraction in ARCHITECTURE.md — there is no per-record incremental file write.
   Every mutating method (`Add`, `IncreaseStock`, `UpdateStatus`, `Enqueue`, `Remove`, etc.)
   follows the pattern: mutate `cache_`, then call `Persist()` which calls `store_.SaveAll(cache_)`
   unconditionally (no partial-write optimization). If a later phase's performance needs change
   this, that's a refactor for that phase, not a concern here.
2. **None of these repositories take an `IClock&` or call `Now()` anywhere.** Time only enters
   `ProductionQueueEntry` as data supplied by the caller (`ProductionService`, in a later phase).
   This phase's repository tests never need a `FakeClock` — they construct `TimePoint` values
   directly (e.g. via `std::chrono::system_clock::time_point` arithmetic on a fixed epoch, or
   whatever concrete `IClock::TimePoint` alias phase-4 settled on) and just check they survive
   storage/serialization unchanged.
3. **Repositories do not know business rules** (valid status transitions, unclaimed-stock
   formula, duplicate-detection beyond primary key, FIFO completion-time chaining) — only storage
   shape and primary-key uniqueness/lookup. This is a deliberate phase boundary: `OrderService`
   and `ProductionService` (both later phases, depending on this one) own the business logic and
   call these repository methods as dumb CRUD/FIFO primitives. Keep repository tests scoped
   accordingly — don't test e.g. "approving with insufficient stock enqueues correctly" here; that
   belongs to the `ProductionService`/`OrderService` phase's own tests.
4. **Test isolation**: every test in this phase's three test files must construct its repository
   against a fresh temp file path (unique per test, e.g. under
   `std::filesystem::temp_directory_path() / "sos_tests" / <unique-name>.json`, cleaned up in a
   RAII fixture or explicit teardown) — never against the real `data/*.json` paths the running
   application would use, and never sharing a path between two unrelated test cases (to avoid
   ordering-dependent test pollution).

## What later phases will depend on from this phase

- `OrderService` (later phase) depends on `OrderRepository::NextOrderNumber`, `Add`,
  `FindByOrderNumber`, `FindByStatus(Reserved)`, `UpdateStatus`, and on `SampleRepository::FindById`,
  `IncreaseStock`/`DecreaseStock`.
- `ProductionService` (later phase) depends on `ProductionQueueRepository::Enqueue`,
  `FindAllInOrder`, `PeekHead`, `Remove`, and on `SampleRepository::IncreaseStock` and
  `OrderRepository::UpdateStatus` (to flip `Producing -> Confirmed` during settlement).
- `MonitoringService`/`ProductionLineViewService` (later phases) depend on
  `OrderRepository::FindAll`/`FindByStatus`, `SampleRepository::FindAll`, and
  `ProductionQueueRepository::FindAllInOrder`/`PeekHead` as read-only sources after settlement.
- Any signature drift from what's declared above (e.g. if `FindByStatus` turns out to need to
  return references instead of copies for performance, or `UpdateStatus` needs to also return the
  updated `Order`) should be treated as this phase's implementer's call to make during TDD, as
  long as it's kept minimal and doesn't leak business-rule validation into the repository layer.
