# Phase 13: Data Monitor & Dummy Data UI

**Depends on:** Phase 5 (repositories), Phase 6 (production-service), Phase 9 (dummy-data-generator)
**Touches:** `SampleOrderSystem/Views/DataMonitorView.h`, `SampleOrderSystem/Views/DataMonitorView.cpp`, `SampleOrderSystem/Controllers/DataMonitorController.h`, `SampleOrderSystem/Controllers/DataMonitorController.cpp`, `SampleOrderSystem/Controllers/DummyDataController.h`, `SampleOrderSystem/Controllers/DummyDataController.cpp`

## Summary

Implement DataMonitorView/DataMonitorController (reads current samples/orders/production-queue JSON state directly via repositories, triggering ProductionService::SettleDueEntries first so the display reflects post-settlement reality) and DummyDataController (invokes DummyDataGenerator without going through the interactive order menus). Depends on repositories (phase-5), ProductionService's settlement entry point (phase-6), and DummyDataGenerator (phase-9, which now itself also depends on phase-6 per the revision above — no change to phase-13's own dependency set since phase-6 was already listed); independent of the Sample/Order/Monitoring UI phases (10-12), so it can proceed in parallel with them once its own dependencies are satisfied.

## Detail

## Scope

This phase adds the last two Controller/View pieces named in the architecture's Console MVC layer: `DataMonitorView`/`DataMonitorController` (a read-only, settle-then-display screen over the three JSON tables) and `DummyDataController` (a thin driver that invokes `DummyDataGenerator` directly, bypassing `OrderController`/`SampleController`'s interactive menus). Nothing here computes business logic — this phase is pure "orchestrate existing services/repositories and render/report the result," matching the architecture's statement that Views do no file I/O and Controllers do no direct JSON/file access.

Do **not** implement `MainMenuController` wiring or the `main.cpp` CLI-flag branch (`--dummy-data`, `--data-monitor`) in this phase — those are cross-cutting entry-point concerns that belong wherever `main.cpp`/`MainMenuController` is assembled (not listed in this phase's `touches`). This phase only needs to produce two `Run()`-style entry points that *that* wiring can call either from a CLI-flag branch or from a menu item, without itself deciding which.

## Testability convention this phase must follow

Neither prior phases nor the architecture doc pin down how console output is captured for tests. To keep this phase's Views/Controllers testable without real console I/O, use this convention (consistent with the "Views render data already fetched by controllers" description):

- `DataMonitorView` takes a `std::ostream&` in its constructor (default-constructible with `std::cout`, but tests pass an `std::ostringstream`). It exposes one pure-rendering method that takes already-fetched, already-settled data — no repository/service references at all.
- `DataMonitorController` takes references to `IClock&`, `ProductionService&`, `SampleRepository&`, `OrderRepository&`, `ProductionQueueRepository&`, and a `DataMonitorView&` (or constructs/owns one internally taking an `std::ostream&` — either is acceptable, but tests need to intercept the rendered text, so whichever shape is chosen, the constructor must accept the destination stream or view instance from the caller, never hardcode `std::cout`).
- `DummyDataController` takes a `DummyDataGenerator&` (or the repositories `DummyDataGenerator` itself needs — see Dependency contract below) plus an `std::ostream&` for its brief confirmation report, and an optional generation-count/seed parameter forwarded to the generator.

If a later phase's actual `SampleView`/`OrderView`/etc. established a different convention (e.g. constructor-injected `std::ostream&` vs. a method-parameter stream), match that convention instead for consistency — but if no such precedent exists yet when this phase is implemented, use the constructor-injection form above.

## `DataMonitorView` (Views/DataMonitorView.h/.cpp)

```cpp
class DataMonitorView {
public:
    explicit DataMonitorView(std::ostream& out = std::cout);

    void Render(const std::vector<Sample>& samples,
                const std::vector<Order>& orders,
                const std::vector<ProductionQueueEntry>& queueEntries);

private:
    std::ostream& out_;
};
```

`Render` prints, in this order, three labeled sections built purely from the vectors passed in (no computation beyond simple counting/formatting):
1. **Samples**: one line per sample — sample ID, name, `currentStock`.
2. **Orders**: one line per order — order number, sample ID, customer, quantity, status (all five statuses shown, including `Rejected` — this is a raw dump of the table, unlike `MonitoringService`'s filtered view, so nothing is excluded here).
3. **Production queue**: one line per entry — order number, sample ID, `shortfallQuantity`, `actualProducedQuantity`, `enqueuedAt`, `expectedCompletionAt` — in file/array order (already FIFO per Key Design Decision #6), not re-sorted.

If a vector is empty, print an explicit "no records" line for that section rather than an empty gap — this is a display-only concern but tests should assert on it so empty-state doesn't silently render as unlabeled blank output.

## `DataMonitorController` (Controllers/DataMonitorController.h/.cpp)

```cpp
class DataMonitorController {
public:
    DataMonitorController(IClock& clock,
                           ProductionService& productionService,
                           SampleRepository& sampleRepository,
                           OrderRepository& orderRepository,
                           ProductionQueueRepository& productionQueueRepository,
                           DataMonitorView& view);

    void Run();
};
```

`Run()` behavior, exactly in this order (mirrors the "Lazy settlement" data-flow section for `MonitoringController`/`ProductionLineViewService`, applied here identically):
1. Call `productionService.SettleDueEntries(clock)` — unconditionally, every call, no flag to skip it. This is what makes the monitor reflect post-settlement reality per the acceptance criterion cited in the phase summary; it must happen even if nothing turns out to be due (idempotent no-op per Key Design Decision #3/architecture line 46).
2. Read all records via `sampleRepository.FindAll()` (or equivalent "list everything" accessor — see note below), `orderRepository.FindAll()`, `productionQueueRepository.FindAll()`.
3. Pass all three straight to `view.Render(samples, orders, queueEntries)` with no filtering/transformation — this view is a raw table dump, distinct from `MonitoringService`'s aggregated counts.

**Dependency-contract note on repository read methods:** phase-5 is expected to expose some "list all records" accessor per repository (`SampleRepository`, `OrderRepository`, `ProductionQueueRepository` each need one — e.g. `FindAll()`/`GetAll()`/`All()`). If phase-5's actual method name differs from `FindAll()`, use whatever it actually named that accessor; the exact name is not load-bearing for any other phase, only its existence and "returns every record in the table, unfiltered" semantics are required here.

## `DummyDataController` (Controllers/DummyDataController.h/.cpp)

```cpp
class DummyDataController {
public:
    DummyDataController(DummyDataGenerator& generator, std::ostream& out = std::cout);

    // sampleCount/orderCount let callers (CLI flag parsing, menu prompt) control
    // how much data to generate; both must have sensible defaults so this is
    // callable with zero arguments beyond Run() itself for a "just generate some data" CLI flag.
    void Run(int sampleCount = 10, int orderCount = 20);
};
```

`Run()` behavior:
1. Call into `DummyDataGenerator` to produce `sampleCount` samples and `orderCount` orders (writing directly through the repositories, per architecture — `DummyDataController` does **not** touch repositories itself, only the generator). The exact generator method signature is phase-9's to define; this phase's controller adapts to whatever phase-9 actually exposes (e.g. a single `Generate(int sampleCount, int orderCount, IClock& clock)` call, or split `GenerateSamples`/`GenerateOrders` calls) — if phase-9 exposes a single combined entry point, prefer calling that one rather than reimplementing sequencing logic here; this controller must not contain any of its own randomization or status-distribution logic, since that belongs entirely to `DummyDataGenerator`.
2. Print a short confirmation summary to `out_` — at minimum the counts actually generated (which may differ from requested if the generator caps/adjusts them) and, if the generator's result type exposes it, a per-status breakdown of generated orders (matching architecture's requirement that generated orders span all five statuses). If `DummyDataGenerator`'s return type doesn't carry this detail yet, it's acceptable for `Run()` to only report the two counts passed in and rely on `DataMonitorController`/existing list views to show the actual generated detail — but do not fabricate numbers not returned by the generator.
3. `DummyDataController::Run` performs no interactive prompting itself — it is meant to be invoked directly from a CLI-flag branch in `main.cpp` or a single non-prompting menu action; if a menu wants to ask the user for counts, that prompting happens in whatever calls `Run(sampleCount, orderCount)`, not inside this method.

## Unit tests to write

All tests should exercise real `SampleRepository`/`OrderRepository`/`ProductionQueueRepository`/`ProductionService` instances pointed at temp-directory JSON files (same pattern phase-5/phase-6 tests presumably already use), with a `FakeClock`, rather than mocking — this phase has no interfaces to mock in front of concrete repository/service classes.

**`DataMonitorView` tests** (pure, no repositories needed — construct fixture vectors directly):
- Renders one line per sample/order/queue-entry with the expected fields present in the output text.
- Empty `samples`/`orders`/`queueEntries` vectors each produce an explicit "no records" line rather than blank/missing output — one test per empty vector, plus a combined all-empty case.
- Rendering does not reorder the `queueEntries` vector — feed entries in a deliberately non-enqueue-time-sorted-looking order and assert output preserves input order (guards against an accidental "helpful" sort creeping in later).
- `Rejected` and `Released` orders appear in the output (this view is unfiltered, unlike `MonitoringService`) — a regression guard against someone copying `MonitoringService`'s exclusion logic in here by mistake.

**`DataMonitorController` tests**:
- Given a queue entry whose `expectedCompletionAt` is already `<= clock.Now()` and a `Producing` order for it, calling `Run()` results in rendered output showing the order as `Confirmed` (not `Producing`) and the sample's stock already incremented — i.e., assert settlement happened *before* rendering, by checking the rendered text reflects post-settlement state, not by mocking `SettleDueEntries` (there's nothing to mock against real repos, so this is the behavioral proof).
- Given a queue entry whose `expectedCompletionAt` is still in the future, `Run()`'s output shows the order still `Producing` and the queue entry still present — proves settlement doesn't over-fire on not-yet-due entries.
- `Run()` called with all three tables empty produces the view's empty-state output (integration of the above View tests through the controller, confirming the "no records propagate" wiring).
- `Run()` called twice in a row (second call after clock hasn't advanced further) produces identical output both times — settlement idempotency surfacing through this controller specifically, not just at the `ProductionService` unit level.

**`DummyDataController` tests**:
- `Run(sampleCount, orderCount)` results in that many (or the generator-adjusted number, whichever the generator contract promises) records actually persisted — assert by re-reading the same repositories after `Run()` returns, not by inspecting the printed summary alone (the summary text is a secondary check).
- Default-argument call `Run()` (no explicit counts) succeeds and produces some non-zero generation — guards the "CLI flag with no extra arguments" invocation path.
- Confirmation output written to the injected `std::ostream&` contains the counts actually generated (assert against the ostringstream contents).
- Calling `Run()` twice in the same process/data directory does not fail or corrupt existing generated records from the first call (append semantics) — a regression guard for the "invocable repeatedly without going through menus" requirement; exact accumulation behavior (does a second call add more on top, or reset?) should be decided by whatever phase-9's generator contract actually promises — if that contract is ambiguous when this phase is implemented, flag it back rather than guessing, since it's phase-9's decision to make, not this phase's.

## What this phase must expose for dependents

No other phase in this plan is listed as depending on phase-13's output (phases 10-12 are explicitly independent/parallel, and this is a leaf UI phase), so `DataMonitorController`/`DummyDataController`'s only real "export" is being wireable from `main.cpp`'s CLI-flag branch and `MainMenuController`'s menu loop, whenever that phase is done. To keep that wiring trivial, both controllers' constructors must take all their dependencies as constructor references/params (no hidden singletons, no static repository access) — `main.cpp` (or whatever phase assembles the menu) only needs to construct one instance of each with already-constructed repositories/services/clock and call `.Run(...)`.

## Files touched (confirmed against phase description)

- `SampleOrderSystem/Views/DataMonitorView.h`, `.cpp` — new
- `SampleOrderSystem/Controllers/DataMonitorController.h`, `.cpp` — new
- `SampleOrderSystem/Controllers/DummyDataController.h`, `.cpp` — new

No existing files are modified by this phase (no `main.cpp`/`MainMenuController` edits) — that wiring is out of scope here and must be covered by whichever phase owns `main.cpp`/menu assembly; if no such phase currently exists in the plan, that's a gap worth surfacing to the planner/caller, since CLAUDE.md's "실행 가능... 대화형 메뉴 전체를 거치지 않고" acceptance criterion (CLI-flag entry points) is only satisfied once something actually calls `DataMonitorController::Run()`/`DummyDataController::Run()` from `main.cpp`.
