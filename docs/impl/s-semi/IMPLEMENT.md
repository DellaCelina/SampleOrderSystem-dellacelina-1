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

## Phases

- [ ] Phase 1: Test project scaffolding + Clock abstraction (deps: none)
- [ ] Phase 2: JSON value/parser/writer (deps: Phase 1)
- [ ] Phase 3: Schema documents + persistence layer (deps: Phase 2)
- [ ] Phase 4: Domain models + ISO-8601 timestamp conversion (deps: Phase 1, Phase 2)
- [ ] Phase 5: Repositories + order-number sequence derivation (deps: Phase 3, Phase 4)
- [ ] Phase 6: ProductionService — shortfall math, FIFO completion time, lazy settlement (deps: Phase 1, Phase 5)
- [ ] Phase 7: OrderService — submit/list/approve/reject/release (deps: Phase 5, Phase 6)
- [ ] Phase 8: MonitoringService + ProductionLineViewService (deps: Phase 5, Phase 6)
- [ ] Phase 9: DummyDataGenerator (deps: Phase 4, Phase 5, Phase 6)
- [ ] Phase 10: Sample UI — SampleView + SampleController (deps: Phase 5)
- [ ] Phase 11: Order UI — OrderView + OrderController (deps: Phase 7)
- [ ] Phase 12: Monitoring & Production Line UI (deps: Phase 8)
- [ ] Phase 13: Data Monitor & Dummy Data UI (deps: Phase 5, Phase 6, Phase 9)
- [ ] Phase 14: MainMenuController + main.cpp wiring and CLI flags (deps: Phase 10, 11, 12, 13)

## Suggested batching (Stage 4)

Batches are maximal groups of not-yet-done phases whose deps are already committed and whose
`touches` don't overlap:

1. **Batch A:** Phase 1 (no deps)
2. **Batch B:** Phase 2, Phase 4 (both only need Phase 1; disjoint files — Json/ vs Models/+Core/Iso8601)
3. **Batch C:** Phase 3 (needs Phase 2)
4. **Batch D:** Phase 5 (needs Phase 3 + Phase 4)
5. **Batch E:** Phase 6, Phase 10 (Phase 6 needs Phase 1+5; Phase 10 needs only Phase 5 — disjoint files: Services/ProductionService vs Views+Controllers/Sample*)
6. **Batch F:** Phase 7, Phase 8 (both need Phase 5+6, disjoint files: OrderService vs Monitoring/ProductionLineViewService)
7. **Batch G:** Phase 9, Phase 11 (Phase 9 needs Phase 4+5+6; Phase 11 needs Phase 7 — disjoint files)
8. **Batch H:** Phase 12, Phase 13 (Phase 12 needs Phase 8; Phase 13 needs Phase 5+6+9 — disjoint files)
9. **Batch I:** Phase 14 (needs 10, 11, 12, 13 — final integration, no parallelism left)

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

## Phase 7: OrderService: submit/list/approve/reject/release

**Depends on:** Phase 5, Phase 6
**Touches:** `SampleOrderSystem/Services/OrderService.h/.cpp`, test vcxproj

`SubmitOrder`, `ListPendingApprovals`, `Approve` (settle-first, then
`max(0, unclaimed)` decision), `Reject`, `Release`. Covers the 50/100/100 acceptance scenario and
settle-then-decide ordering. Full detail:
[phase-07-order-service/DETAIL.md](phase-07-order-service/DETAIL.md).

## Phase 8: MonitoringService + ProductionLineViewService

**Depends on:** Phase 5, Phase 6
**Touches:** `SampleOrderSystem/Services/MonitoringService.h/.cpp`, `ProductionLineViewService.h/.cpp`, test vcxproj

Both settle-first, read-only query services; no interdependency with each other or with
Phase 7 — can run in parallel with it. Full detail:
[phase-08-monitoring-production-view-service/DETAIL.md](phase-08-monitoring-production-view-service/DETAIL.md).

## Phase 9: DummyDataGenerator

**Depends on:** Phase 4, Phase 5, Phase 6
**Touches:** `SampleOrderSystem/Services/DummyDataGenerator.h/.cpp`, test vcxproj

Schema-driven random Sample/Order generation across all five statuses, writing directly through
repositories (not through OrderService/ProductionService orchestration methods) but **calling
ProductionService's pure compute functions** (`ComputeShortfall`/`ComputeActualQuantity`/
`ComputeCompletionTime`) to keep generated `Producing` orders' queue entries consistent with real
ones — this is why Phase 9 genuinely depends on Phase 6, not just Phase 4/5. Can still proceed in
parallel with Phase 7/8. Full detail:
[phase-09-dummy-data-generator/DETAIL.md](phase-09-dummy-data-generator/DETAIL.md).

## Phase 10: Sample UI (SampleView + SampleController)

**Depends on:** Phase 5
**Touches:** `SampleOrderSystem/Views/SampleView.h/.cpp`, `Controllers/SampleController.h/.cpp`

Console rendering + input handling for sample registration and list/search, calling
`SampleRepository` directly. Independent of Order/Production/Monitoring — parallelizable with
Phases 6-9 and 11-13. Full detail: [phase-10-sample-ui/DETAIL.md](phase-10-sample-ui/DETAIL.md).

## Phase 11: Order UI (OrderView + OrderController)

**Depends on:** Phase 7
**Touches:** `SampleOrderSystem/Views/OrderView.h/.cpp`, `Controllers/OrderController.h/.cpp`

Console rendering + controller for submission/pending-list/approve/reject/release, wired to
`OrderService`. Independent of Phase 10/12/13. Full detail:
[phase-11-order-ui/DETAIL.md](phase-11-order-ui/DETAIL.md).

## Phase 12: Monitoring & Production Line UI

**Depends on:** Phase 8
**Touches:** `SampleOrderSystem/Views/MonitoringView.h/.cpp`, `ProductionLineView.h/.cpp`, `Controllers/MonitoringController.h/.cpp`

Console views for status counts, stock/Depleted-InStock labels, and production-line head+tail
display, plus the controller calling `MonitoringService`/`ProductionLineViewService`. Independent
of Phases 10, 11, 13. Full detail:
[phase-12-monitoring-production-ui/DETAIL.md](phase-12-monitoring-production-ui/DETAIL.md).

## Phase 13: Data Monitor & Dummy Data UI

**Depends on:** Phase 5, Phase 6, Phase 9
**Touches:** `SampleOrderSystem/Views/DataMonitorView.h/.cpp`, `Controllers/DataMonitorController.h/.cpp`, `Controllers/DummyDataController.h/.cpp`

`DataMonitorView`/`DataMonitorController` (settle-first, then render current JSON state) and
`DummyDataController` (invokes `DummyDataGenerator` outside the interactive menus). Independent of
Phases 10-12. Full detail:
[phase-13-data-monitor-dummy-ui/DETAIL.md](phase-13-data-monitor-dummy-ui/DETAIL.md).

## Phase 14: MainMenuController + main.cpp wiring and CLI flags

**Depends on:** Phase 10, Phase 11, Phase 12, Phase 13
**Touches:** `SampleOrderSystem/Controllers/MainMenuController.h/.cpp`, `SampleOrderSystem/main.cpp`

Wires everything together: constructs `SystemClock` + all repositories/services, exposes every
feature via `MainMenuController`'s interactive loop, and supports `--dummy-data`/`--data-monitor`
CLI flags that run directly and exit. Pins down `data/`/`schema/` resolution relative to the
executable path. Final integration point — no parallelism left after this. Full detail:
[phase-14-main-wiring/DETAIL.md](phase-14-main-wiring/DETAIL.md).
