# Implementation Plan: S-Semi 반도체 시료 생산주문관리 시스템

Implements: [docs/ARCHITECTURE.md](../../ARCHITECTURE.md)

Each phase's full TDD-ready detail (edge cases, exact signatures, test scenarios) lives in that
phase's own `DETAIL.md` in this directory; this file tracks the phase checklist and dependency
graph only. Each phase also has its own `STATUS.md` to resume mid-implementation across sessions.

**Resolved before this plan was built:** ARCHITECTURE.md's Open Question #1 (whether the "no new
VS project files" constraint bans a `SampleOrderCore` static library) is resolved in favor of the
literal reading already adopted by ARCHITECTURE.md's Key Design Decision #1 — no library project,
`SampleOrderSystemTests.vcxproj` compiles the app's non-UI sources a second time via direct
relative-path `<ClCompile>`/`<ClInclude>` items. Phase 1 assumes this; it is not an open decision
blocking implementation.

**Test framework superseded mid-implementation:** the user has installed the `gmock` NuGet package
(v1.11.0, bundling GoogleTest + GoogleMock) into both `SampleOrderSystem.vcxproj` and
`SampleOrderSystemTests.vcxproj` and requires GoogleTest/GoogleMock for all tests. This supersedes
every phase DETAIL.md's original Catch2-based test framework references (Key Design Decision #7 in
ARCHITECTURE.md and phase-1's DETAIL.md are updated accordingly) — wherever a phase's DETAIL.md
still says `TEST_CASE`/`SECTION`/`REQUIRE`/Catch2, read it as `TEST`/`EXPECT_*`/`ASSERT_*`
GoogleTest syntax instead (and use GoogleMock `MOCK_METHOD`/matchers wherever a phase needs to mock
a collaborator, e.g. `IClock` or a repository, rather than hand-rolling a fake). Phase 1 and
Phase 2's existing tests have been converted; every phase from here on is written directly against
GoogleTest/GoogleMock.

## Phases

- [x] Phase 1: Test project scaffolding + Clock abstraction (deps: none)
- [x] Phase 2: JSON value/parser/writer (deps: Phase 1)
- [x] Phase 3: Schema documents + persistence layer (deps: Phase 2)
- [x] Phase 4: Domain models + ISO-8601 timestamp conversion (deps: Phase 1, Phase 2)
- [x] Phase 5: Repositories + order-number sequence derivation (deps: Phase 3, Phase 4)
- [x] Phase 6: ProductionService — shortfall math, FIFO completion time, lazy settlement (deps: Phase 1, Phase 5)
- [x] Phase 7 (consolidated): Remaining services — OrderService, MonitoringService, ProductionLineViewService, DummyDataGenerator (deps: Phase 5, Phase 6)
- [x] Phase 8 (consolidated): UI — Sample UI (already implemented, merged in from the old standalone Phase 10), Order UI, Monitoring & Production Line UI, Data Monitor & Dummy Data UI (deps: Phase 5, Phase 7)
- [x] Phase 9 (consolidated): Main wiring — MainMenuController + main.cpp + CLI flags (deps: Phase 8)

**Consolidation note (post Phase 6/10):** the remaining work was originally split into 7 finer
phases (old 7/8/9/11/12/13/14) for maximal parallelism, but per explicit user direction the
per-phase agent overhead was cut by merging what's left into 3 larger phases, run one at a time
(services -> UI -> main), each still as its own single Red -> Green -> Refactor -> commit -> review
cycle. The old phase numbers/detail docs are kept as-is and referenced below rather than rewritten,
since their acceptance-criteria-level detail is still accurate — only the batching/grouping changed.

## Suggested batching (Stage 4)

Batches are maximal groups of not-yet-done phases whose deps are already committed and whose
`touches` don't overlap:

1. **Batch A:** Phase 1 (no deps)
2. **Batch B:** Phase 2, Phase 4 (both only need Phase 1; disjoint files — Json/ vs Models/+Core/Iso8601)
3. **Batch C:** Phase 3 (needs Phase 2)
4. **Batch D:** Phase 5 (needs Phase 3 + Phase 4)
5. **Batch E:** Phase 6, Phase 10 (Phase 6 needs Phase 1+5; Phase 10 needs only Phase 5 — disjoint files: Services/ProductionService vs Views+Controllers/Sample*). Phase 10 was later folded into Phase 8 as a bookkeeping merge (see below) since it's UI-shaped work, but it was implemented and committed here, in this batch, before that merge happened.
6. **Batch F:** Phase 7 consolidated — OrderService + MonitoringService/ProductionLineViewService + DummyDataGenerator, implemented as one sequential phase (needs Phase 5+6; no longer split across parallel agents)
7. **Batch G:** Phase 8 consolidated — Sample UI (already done, merged in from old Phase 10) + Order UI + Monitoring/ProductionLine UI + DataMonitor/DummyData UI, one sequential phase for the three still-pending sub-features (needs Phase 5+7)
8. **Batch H:** Phase 9 consolidated — MainMenuController + main.cpp wiring (needs Phase 8 — final integration, no parallelism left)

All phases share `SampleOrderSystemTests.vcxproj` as a `touches` entry (each phase adds its new
source files to it). This is a known, accepted overlap — set `overlappingFiles: true` for any
batch containing more than one phase, since the shared vcxproj item-list edits are exactly the
kind of "looks disjoint on paper but isn't" case the batch-implement workflow's worktree-isolation
option exists for.

## Phase 1: Test project scaffolding + Clock abstraction

**Depends on:** none
**Touches:** `SampleOrderSystem.slnx`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`, `SampleOrderSystemTests/catch_amalgamated.hpp`, `SampleOrderSystemTests/catch_amalgamated.cpp`, `SampleOrderSystem/Core/IClock.h`, `SampleOrderSystem/Core/SystemClock.h`, `SampleOrderSystem/Core/SystemClock.cpp`, `SampleOrderSystemTests/FakeClock.h`

Stand up `SampleOrderSystemTests.vcxproj` (native Catch2 v3 test project referenced from
`SampleOrderSystem.slnx`, vendoring `catch_amalgamated.hpp`/`.cpp`), get one trivial passing test
compiling/linking, and implement `IClock` / `SystemClock` / `FakeClock`. Pure infrastructure —
every later phase's TDD cycle depends on this compiling and running. Full detail:
[phase-01-test-scaffolding-clock/DETAIL.md](phase-01-test-scaffolding-clock/DETAIL.md).

## Phase 2: JSON value/parser/writer

**Depends on:** Phase 1
**Touches:** `SampleOrderSystem/Json/JsonValue.h/.cpp`, `JsonParser.h/.cpp`, `JsonWriter.h/.cpp`, test vcxproj

Recursive `JsonValue` variant (Null/Bool/Number/String/Array/Object), `JsonParser` (throws
`JsonParseException` on malformed input), `JsonWriter` (pretty-print). Standalone JSON library, no
schema/domain knowledge. Full detail:
[phase-02-json-value-parser-writer/DETAIL.md](phase-02-json-value-parser-writer/DETAIL.md).

## Phase 3: Schema documents + persistence layer (JsonFileStore, SchemaValidator)

**Depends on:** Phase 2
**Touches:** `schema/*.schema.json`, `SampleOrderSystem/Persistence/Schema.h`, `SchemaValidator.h/.cpp`, `JsonFileStore.h/.cpp`, test vcxproj

Commits the three schema documents, `Schema` descriptor structs, `SchemaValidator` (own
self-contained ISO-8601 format check, independent of Phase 4's conversion functions), and
`JsonFileStore` with whole-table fail-fast load semantics. Full detail:
[phase-03-schema-persistence/DETAIL.md](phase-03-schema-persistence/DETAIL.md).

## Phase 4: Domain models + ISO-8601 timestamp conversion

**Depends on:** Phase 1, Phase 2
**Touches:** `SampleOrderSystem/Models/Sample.h/.cpp`, `Order.h/.cpp`, `ProductionQueueEntry.h/.cpp`, `SampleOrderSystem/Core/Iso8601.h/.cpp`, test vcxproj

`Sample`/`Order`/`ProductionQueueEntry` structs + `ToJson`/`FromJson`, plus the single
`TimePointToIso8601`/`ParseIso8601` pair in `Core/Iso8601.h/.cpp` used by every model. Independent
of Phase 3 — can run in parallel. Full detail:
[phase-04-domain-models-iso8601/DETAIL.md](phase-04-domain-models-iso8601/DETAIL.md).

## Phase 5: Repositories (Sample/Order/ProductionQueue) + order-number sequence derivation

**Depends on:** Phase 3, Phase 4
**Touches:** `SampleOrderSystem/Repositories/SampleRepository.h/.cpp`, `OrderRepository.h/.cpp`, `ProductionQueueRepository.h/.cpp`, `data/*.json`, test vcxproj

`SampleRepository` (CRUD, exact/substring find, stock mutation), `OrderRepository` (append-only,
status updates, restart-safe `NextOrderNumber()` re-derived from `data/orders.json` at load time),
`ProductionQueueRepository` (FIFO = file-array order). First phase combining persistence + models.
Full detail: [phase-05-repositories/DETAIL.md](phase-05-repositories/DETAIL.md).

## Phase 6: ProductionService: shortfall math, FIFO completion time, lazy settlement

**Depends on:** Phase 1, Phase 5
**Touches:** `SampleOrderSystem/Services/ProductionService.h/.cpp`, test vcxproj

`ComputeShortfall`, `ComputeActualQuantity` (`ceil(shortfall/yield)`), `ComputeCompletionTime`
(FIFO chain against current queue tail), `Enqueue`, `SettleDueEntries(IClock&)` — the single lazy
reconciliation entry point. These three compute functions must be exposed as independently
callable pure functions (not private helpers), since Phase 9 (DummyDataGenerator) must call the
exact same math. Core production-domain logic — must land before Phase 7/8/9. Full detail:
[phase-06-production-service/DETAIL.md](phase-06-production-service/DETAIL.md).

## Phase 7 (consolidated): Remaining services — OrderService, MonitoringService, ProductionLineViewService, DummyDataGenerator

**Depends on:** Phase 5, Phase 6
**Touches:** `SampleOrderSystem/Services/OrderService.h/.cpp`, `MonitoringService.h/.cpp`, `ProductionLineViewService.h/.cpp`, `DummyDataGenerator.h/.cpp`, test vcxproj

Merges the old Phase 7/8/9 into one sequential implementation pass (same file scope, run as a
single Red -> Green -> Refactor chain instead of three parallel ones, per the consolidation note
above):

- **OrderService** — `SubmitOrder`, `ListPendingApprovals`, `Approve` (settle-first, then
  `max(0, unclaimed)` decision), `Reject`, `Release`. Covers the 50/100/100 acceptance scenario and
  settle-then-decide ordering. Full detail: [phase-07-services/DETAIL-order-service.md](phase-07-services/DETAIL-order-service.md).
- **MonitoringService + ProductionLineViewService** — both settle-first, read-only query services.
  Full detail: [phase-07-services/DETAIL-monitoring-production-view-service.md](phase-07-services/DETAIL-monitoring-production-view-service.md).
- **DummyDataGenerator** — schema-driven random Sample/Order generation across all five statuses,
  writing directly through repositories but **calling ProductionService's pure compute functions**
  (`ComputeShortfall`/`ComputeActualQuantity`/`ComputeCompletionTime`) to keep generated `Producing`
  orders' queue entries consistent with real ones. Full detail:
  [phase-07-services/DETAIL-dummy-data-generator.md](phase-07-services/DETAIL-dummy-data-generator.md).

## Phase 8 (consolidated): Remaining UI — Order UI, Monitoring & Production Line UI, Data Monitor & Dummy Data UI

**Depends on:** Phase 7
**Touches:** `SampleOrderSystem/Views/OrderView.h/.cpp`, `MonitoringView.h/.cpp`, `ProductionLineView.h/.cpp`, `DataMonitorView.h/.cpp`, `Controllers/OrderController.h/.cpp`, `MonitoringController.h/.cpp`, `DataMonitorController.h/.cpp`, `DummyDataController.h/.cpp`, test vcxproj

Merges the old Phase 11/12/13 into one sequential implementation pass:

- **Order UI** — console rendering + controller for submission/pending-list/approve/reject/release,
  wired to `OrderService`. Full detail: [phase-08-ui/DETAIL-order-ui.md](phase-08-ui/DETAIL-order-ui.md).
- **Monitoring & Production Line UI** — console views for status counts, stock/Depleted-InStock
  labels, and production-line head+tail display, plus the controller calling
  `MonitoringService`/`ProductionLineViewService`. Full detail:
  [phase-08-ui/DETAIL-monitoring-production-ui.md](phase-08-ui/DETAIL-monitoring-production-ui.md).
- **Data Monitor & Dummy Data UI** — `DataMonitorView`/`DataMonitorController` (settle-first, then
  render current JSON state) and `DummyDataController` (invokes `DummyDataGenerator` outside the
  interactive menus). Full detail:
  [phase-08-ui/DETAIL-data-monitor-dummy-ui.md](phase-08-ui/DETAIL-data-monitor-dummy-ui.md).

## Phase 9 (consolidated): Main wiring — MainMenuController + main.cpp + CLI flags

**Depends on:** Phase 8
**Touches:** `SampleOrderSystem/Controllers/MainMenuController.h/.cpp`, `SampleOrderSystem/main.cpp`

Same scope as the old Phase 14: wires everything together — constructs `SystemClock` + all
repositories/services, exposes every feature via `MainMenuController`'s interactive loop, and
supports `--dummy-data`/`--data-monitor` CLI flags that run directly and exit. Pins down
`data/`/`schema/` resolution relative to the executable path. Final integration point — no
parallelism left after this. Full detail: [phase-09-main-wiring/DETAIL.md](phase-09-main-wiring/DETAIL.md).

**Process note:** per explicit user direction, from this phase onward the TDD Red/Green/Refactor
cycle is skipped — implement directly (Green-only), verify by building both projects and running
the app manually, and skip the pc-reviewer gate too unless the user asks for it again.
