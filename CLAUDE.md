# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Console C++ application: a semiconductor sample production/order management system ("S-Semi").
Customers request samples, an order handler registers orders, a production handler approves/rejects
them and runs a production line for stock shortfalls. Full functional spec lives in
`docs/PREREQUIREMENT.md.txt` and `[CRA_AI] Day3_개인과제_반도체시료관리.pdf` — read both before
designing or implementing anything; they are the source of truth for behavior, not this file.

No application source exists yet — the repo currently contains only an empty Visual Studio C++
console project skeleton (`SampleOrderSystem/SampleOrderSystem.vcxproj`).

## Build

This is a Visual Studio C++ project (`SampleOrderSystem.slnx`), not CMake — no Makefile/CMakeLists.
- Language standard: C++20 (`stdcpp20`), Unicode character set, console subsystem.
- Configurations: Debug/Release x Win32/x64. Platform toolset `v145`.
- Two projects: `SampleOrderSystem/SampleOrderSystem.vcxproj` (the console app) and
  `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj` (GoogleTest/GoogleMock, via the NuGet
  `gmock` package — compiles the app's non-UI sources a second time via relative-path
  `<ClCompile>`/`<ClInclude>` items, no shared library project). **Building only the tests project
  does not prove the app project still compiles** — a wrong `#include` style can compile by
  accident in the tests project (it has an extra `AdditionalIncludeDirectories` entry) while
  breaking the real app outright, so build both when in doubt (see Repo-specific conventions below).

**From this repo's git-bash shell** (`MSYS2_ARG_CONV_EXCL` prevents `/p:`/`/t:` flags from being
mangled to Windows paths):
```bash
export MSYS2_ARG_CONV_EXCL="*"
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" \
  SampleOrderSystemTests/SampleOrderSystemTests.vcxproj \
  /p:Configuration=Debug /p:Platform=x64 /m:1 /v:minimal
# add /t:Rebuild for a from-scratch build; swap the project path to build the app instead:
#   SampleOrderSystem/SampleOrderSystem.vcxproj
```
Run the tests directly afterward (don't just trust a subagent's summary of the run):
```bash
./SampleOrderSystemTests/x64/Debug/SampleOrderSystemTests.exe
# one suite/test only, e.g. while iterating on a single phase:
./SampleOrderSystemTests/x64/Debug/SampleOrderSystemTests.exe --gtest_filter=OrderServiceTest.*
./SampleOrderSystemTests/x64/Debug/SampleOrderSystemTests.exe --gtest_filter=OrderServiceTest.RejectOfAReservedOrderSucceedsAndRemovesItFromPendingApprovals
```
Look for `[  PASSED  ] N tests.` at the end — a `[  FAILED  ]` block lists the specific failing
test names above it.
The app project (`SampleOrderSystem.vcxproj`) is expected to fail to **link** with
`LNK2019: unresolved external symbol main` until the main-wiring phase adds `main.cpp` — that's a
known, pre-existing gap, not a regression; a *compile* error there (anything before the link step)
is real and must be fixed.

**Don't build in parallel with a background implementation workflow still writing files** — two
concurrent MSBuild invocations against the same project can collide on the shared `.pdb`
(`C1041: cannot open program database ... use /FS`), and a build kicked off mid-write will also hit
missing-source-file errors for files the agent hasn't created yet. Wait for the workflow's
completion notification first, then build.

## Repo-specific conventions

- **Global namespace everywhere** — no `SampleOrderSystem::` (or any) wrapper on any class/function.
- **Include style**: a `.cpp` includes its own header by bare filename only (e.g.
  `Services/OrderService.cpp` → `#include "OrderService.h"`), and any cross-folder include is
  relative with `../` (e.g. `#include "../Repositories/OrderRepository.h"`,
  `#include "../Core/IClock.h"`). Bare project-root-style includes (`"Services/X.h"`, `"Core/Y.h"`)
  from inside a subfolder only compile by accident in the tests project (see Build above) and break
  the real app project — this has been a real, repeated bug source.
- `OrderStatus` serializes to JSON as **UPPER-CASE** strings (`RESERVED`/`CONFIRMED`/`PRODUCING`/
  `RELEASED`/`REJECTED`) via `OrderStatusToString`/`OrderStatusFromString` — never hardcode a
  different casing in code, tests, or schema files (a real critical bug here broke schema
  validation for every real order).
- Test framework is **GoogleTest/GoogleMock** (`TEST`/`TEST_F`, `EXPECT_*`/`ASSERT_*`,
  `MOCK_METHOD`), not Catch2.

## Domain model (from the spec)

**Order states**: `RESERVED` → (approve) → `CONFIRMED` (stock sufficient) or `PRODUCING` (stock
short, queued to the production line) → `RELEASED` (shipped). Or `RESERVED` → (reject) →
`REJECTED` (terminal, excluded from monitoring). `PRODUCING` → `CONFIRMED` automatically when
production for that order finishes.

**Order numbering**: `ORD-####` (4-digit sequence). Samples have both a sample ID and a name.

**Stock vs. availability are distinct**: current stock includes quantity already claimed by
`PRODUCING`/`CONFIRMED` orders. When approving a new order, only the *unclaimed* remainder counts
as "sufficient stock" — already-reserved-by-other-orders stock is not double-allocated.

**Production queue semantics** (this is the part most likely to be gotten wrong — re-read
`docs/PREREQUIREMENT.md.txt` lines 7–20 before touching this logic):
- The queue is FIFO and persisted to a file, storing each production entry plus its expected
  completion time.
- Queue state is only reconciled lazily, at query time — whenever *any* order status, production
  line status, or stock is queried, first sweep the file for entries whose completion time has
  passed, remove them, and update the corresponding order status (`PRODUCING`→`CONFIRMED`) and
  stock levels accordingly. There is no background timer/thread.
- Only the shortfall computed at approval time enters the queue — not the full order quantity, and
  not quantities from other still-pending/queued orders. Orders for the same sample placed after
  an earlier one may reach `CONFIRMED` before it if stock produced in between happens to cover them.
- Actual production yield is not simulated: real quantity produced = `ceil(shortfall / yield)`,
  and in this system 100% of that produced quantity survives (so stock can end up with surplus
  beyond what any order needs).
- Total production time for a queue entry = `average production time * actual quantity produced`.

**Sample attributes**: sample ID, name, average production time, yield (0–1, e.g. 0.9 = 90
survive per 100 produced).

## Implementation guide (from the spec — external repos to mirror, not copy verbatim)

The assignment requires porting patterns from three sibling PoC repos the author built separately;
consult these repos' structure/approach when implementing the corresponding piece, don't invent an
unrelated design:
- JSON parsing/file persistence: `DataPersistence-dellacelina-1`
- MVC console UI structure: `ConsoleMVC-dellacelina-1`
- JSON-as-database data management: `DataMonitor-dellacelina-1`
- Dummy/test data generation: `DummyDataGenerator-dellacelina-1`

All persistent state (orders, samples/stock, production queue) is stored as JSON files, read/written
directly — no external database.

## Working process for this repo

This repo has a `project-cycle` skill configured (`.claude/skills/project-cycle/SKILL.md`) that
runs feature work through a gated Requirement → Architecture → Phase Plan → TDD Implementation →
Final Review lifecycle via subagent workflows, with a human review gate between each stage. Use it
for building out this system rather than writing ad hoc code — check current stage from
`docs/` + git state each time, per that skill's own instructions, rather than assuming from memory.

## Docs

- `docs/REQUIREMENT.md` — Stage 1 output: checkable acceptance criteria for the first end-to-end
  implementation of this system.
- `docs/ARCHITECTURE.md` — Stage 2 output: component design, data flow, and key design decisions
  grounded in the real repo, satisfying `docs/REQUIREMENT.md`.
- `docs/impl/s-semi/IMPLEMENT.md` — Stage 3 output: 14-phase dependency-aware TDD implementation
  plan satisfying `docs/ARCHITECTURE.md`; each phase's full detail and status live alongside it.
