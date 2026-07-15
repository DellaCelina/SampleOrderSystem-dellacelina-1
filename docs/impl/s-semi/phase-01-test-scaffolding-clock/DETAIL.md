# Phase 1: Test project scaffolding + Clock abstraction

**Depends on:** none
**Touches:** `SampleOrderSystem.slnx`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`, `SampleOrderSystemTests/catch_amalgamated.hpp`, `SampleOrderSystemTests/catch_amalgamated.cpp`, `SampleOrderSystem/Core/IClock.h`, `SampleOrderSystem/Core/SystemClock.h`, `SampleOrderSystem/Core/SystemClock.cpp`, `SampleOrderSystemTests/FakeClock.h`

## Summary

OPEN QUESTION #1 MUST BE CONFIRMED BY THE HUMAN REVIEWER BEFORE THIS PHASE IS IMPLEMENTED: the architecture leaves open whether the no-new-VS-project-files constraint bans a SampleOrderCore static library or only a second .exe, and explicitly says this must be resolved before the phase plan commits to either the shared-sources-compiled-twice approach or a library split. This phase plan assumes (pending that sign-off) the shared-sources answer: SampleOrderSystemTests.vcxproj adds explicit <ClCompile>/<ClInclude> items by relative path at SampleOrderSystem's non-UI sources, compiling them a second time into the test binary rather than linking a shared static library. If the reviewer instead picks the library-split answer, this phase's mechanism (and every later phase's vcxproj-maintenance step, see phases 2-9) changes and must be replanned before work starts — do not proceed past this phase without that confirmation being recorded. Once confirmed, stand up the native Visual Studio test project referenced from SampleOrderSystem.slnx, vendor Catch2 v3 as catch_amalgamated.hpp/.cpp, and get a trivial passing test compiling/linking against SampleOrderSystem's non-UI sources. Also implement the IClock abstract interface, its SystemClock production implementation, and a FakeClock test double whose time is advanced explicitly. This phase is pure infrastructure: every later phase's TDD cycle depends on the test project compiling and running, and Services/Repositories/tests throughout the plan take an IClock& rather than reading the wall clock directly.

## Superseded: test framework switched from Catch2 to GoogleTest/GoogleMock

Everything below describing vendoring Catch2 (`catch_amalgamated.hpp`/`.cpp`, `CATCH_CONFIG_MAIN`,
`TEST_CASE`/`SECTION`/`REQUIRE`) is historical — the user has since installed the `gmock` NuGet
package (v1.11.0, bundling GoogleTest + GoogleMock) into both `SampleOrderSystem.vcxproj` and
`SampleOrderSystemTests.vcxproj` via `packages.config`, and requires GoogleTest/GoogleMock for all
tests, retroactively including this phase's. The actual, current state of this phase's deliverable:

- `catch_amalgamated.hpp`/`.cpp` were removed; GoogleTest/GoogleMock are brought in via the `gmock`
  NuGet package's `build/native/gmock.targets` import (already wired into both `.vcxproj` files —
  it adds the include path and compiles `gtest-all.cc`/`gmock-all.cc` automatically). No files need
  vendoring or manual downloading.
- `SampleOrderSystemTests/TestMain.cpp` supplies `main()`:
  `::testing::InitGoogleMock(&argc, argv); return RUN_ALL_TESTS();` — replaces Catch2's built-in
  runner.
- `ClockTests.cpp` was converted from `TEST_CASE`/`REQUIRE`/`SECTION` to GoogleTest's
  `TEST(Suite, Case)` / `EXPECT_EQ`/`EXPECT_TRUE`/`EXPECT_THROW` (each former `SECTION` became its
  own `TEST`, since GoogleTest has no direct `SECTION` equivalent and each section here was
  independent, fresh-state test logic anyway).
- `IClock`/`SystemClock`/`FakeClock` themselves are unchanged — this was purely a test-harness
  swap, not a production-code change.

The still-useful parts of the narrative below (the `IClock`/`SystemClock`/`FakeClock` design,
namespace convention, test *scenarios* to cover) remain accurate; only the literal Catch2 API
calls and vcxproj vendoring mechanics are superseded.

## Detail

## OPEN QUESTION #1 — must be confirmed by the human reviewer before this phase is implemented

`docs/ARCHITECTURE.md` explicitly leaves open whether the "no new VS project files beyond what's needed" / "no new .exe" constraint on this repo also bans introducing a `SampleOrderCore` **static library** project, or whether it only bans a second **executable**. This phase cannot be safely implemented until that is resolved, because the answer determines the physical mechanism for sharing code between `SampleOrderSystem.vcxproj` (the app) and `SampleOrderSystemTests.vcxproj` (this phase's new test project):

- **Answer A — shared-sources-compiled-twice (assumed default for this plan):** `SampleOrderSystemTests.vcxproj` lists the app's non-UI `.cpp`/`.h` files as explicit `<ClCompile>`/`<ClInclude>` items by relative path (e.g. `..\SampleOrderSystem\Core\SystemClock.cpp`), and both projects compile those same source files independently into two separate object sets/binaries. No new library project is created; `SampleOrderSystem.vcxproj` is untouched other than being referenced from the `.slnx`.
- **Answer B — library split:** `SampleOrderSystem`'s non-UI code is pulled out into a new `SampleOrderCore` static library project, `SampleOrderSystem.exe` links against it, and `SampleOrderSystemTests.exe` links against it too. This is cleaner (single compilation of shared code, no drift risk between the two `<ClCompile>` item lists) but requires creating a new project type the architecture flagged as possibly disallowed.

This phase plan is written assuming **Answer A** is confirmed. If the reviewer instead picks **Answer B**, the concrete steps below (and the "add new file → remember to add it to both vcxproj item lists" maintenance step baked into every later phase, phases 2–9) must be replanned before any TDD work starts on this phase. Do not begin implementation until this is recorded as resolved in the human review gate for this phase.

Everything below assumes Answer A.

## Goal of this phase

Stand up a working native C++ test project that:
1. Is referenced from `SampleOrderSystem.slnx` and builds via the same MSBuild invocation used for the app (`msbuild SampleOrderSystem.slnx /p:Configuration=Debug /p:Platform=x64`, and ideally also `Win32`).
2. Vendors Catch2 v3 as a single-header/single-source amalgamation (no package manager dependency — this repo has no vcpkg/NuGet Catch2 reference yet, and adding one is out of scope for this phase; vendoring keeps the build self-contained).
3. Compiles and passes one trivial smoke test, proving the harness works end to end (discovery, run, exit code).
4. Compiles a copy of `SampleOrderSystem`'s non-UI sources into the test binary (per Answer A), proving the shared-sources approach actually links, even though at this point there is only `Core/SystemClock.cpp` to compile.
5. Delivers the `IClock` / `SystemClock` / `FakeClock` abstraction that every later phase's services and tests will depend on for testable time.

## Step-by-step scaffolding

### 1. `SampleOrderSystemTests.vcxproj`

Create a new console-app-style vcxproj (Catch2's own `main()` via `catch_amalgamated.cpp` with `CATCH_CONFIG_MAIN`... actually prefer `CATCH_CONFIG_RUNNER` off — just let Catch2 supply `main`, i.e. do **not** define `CATCH_AMALGAMATED_CUSTOM_MAIN`, so `catch_amalgamated.cpp` itself provides the `main()` entry point):
- Mirror the app project's settings: C++20 (`stdcpp20`), Unicode character set, console subsystem, Debug/Release x Win32/x64 configurations, toolset `v145`.
- `<ItemGroup>` with explicit `<ClInclude>`/`<ClCompile>` for:
  - `catch_amalgamated.hpp`, `catch_amalgamated.cpp` (vendored, see step 2)
  - `FakeClock.h` (test-only double, lives in the test project, not the app)
  - Every test `.cpp` file added over the life of the plan, starting with `ClockTests.cpp` and a `SmokeTest.cpp` (or fold the smoke test into `ClockTests.cpp` — see tests below)
  - The app's shared non-UI sources compiled a second time, by relative path: at minimum `..\SampleOrderSystem\Core\IClock.h`, `..\SampleOrderSystem\Core\SystemClock.h`, `..\SampleOrderSystem\Core\SystemClock.cpp`. (Every later phase that adds a new non-UI `.h`/`.cpp` to the app project must add the same file here too — call this out explicitly as a recurring "vcxproj maintenance" step in phases 2–9's plans.)
  - Include path: add `..\SampleOrderSystem` (or the specific `Core` subfolder path) to `AdditionalIncludeDirectories` so `#include "Core/IClock.h"` resolves the same way from both projects.
- No `<ProjectReference>` to `SampleOrderSystem.vcxproj` is needed or wanted under Answer A — the test project must **not** depend on `SampleOrderSystem.exe` building or linking (that project has a UI entry point `main()`/`WinMain` that would collide with Catch2's `main()` if actually linked together; keeping them as separate binaries compiling shared sources is the whole point of Answer A).
- Add `SampleOrderSystemTests.vcxproj` to `SampleOrderSystem.slnx`'s project list and to all four Debug/Release x Win32/x64 solution configurations, `Build.0` mapped normally.

### 2. Vendor Catch2 v3

- Download/copy the official single-file amalgamated distribution (`extras/catch_amalgamated.hpp` and `extras/catch_amalgamated.cpp` from the Catch2 v3 release) into `SampleOrderSystemTests/`. Pin a specific released version (record the version number in a comment at the top of `catch_amalgamated.hpp` or in a short `SampleOrderSystemTests/CATCH2_VERSION.txt` — pick whichever is cheaper; a version-number comment in the header is sufficient and avoids an extra file) so future upgrades are a deliberate, visible diff.
- No other Catch2 files are needed — the amalgamated pair is self-contained (no `<catch2/catch_all.hpp>` multi-file include tree).

### 3. Trivial smoke test

In `SampleOrderSystemTests/ClockTests.cpp` (this phase's one test file — combining the smoke test and the Clock tests avoids an extra near-empty file):
```cpp
#include "catch_amalgamated.hpp"

TEST_CASE("test harness is wired up", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}
```
This proves: project compiles, links, Catch2's `main()` runs, discovers the case, executes it, returns exit code 0 on success / non-zero on failure. Confirm both `msbuild` build and running the resulting `.exe` directly (or via `vstest.console`/plain process execution — no CI runner is being wired up in this phase, just confirm the binary runs and reports pass/fail via its own console output and exit code) work for at least one configuration (Debug x64) before considering this phase done; Release and Win32 should also be confirmed if convenient but are not blocking if Debug x64 is green (note this as a residual risk if skipped, since the repo's own instructions call out four configurations).

### 4. `IClock` interface

`SampleOrderSystem/Core/IClock.h`:
```cpp
#pragma once
#include <chrono>

// Abstraction over wall-clock time so that any component reasoning about
// elapsed time or "has this deadline passed" (production queue completion
// sweeps, order timestamps, etc.) can be driven by a fake clock in tests
// instead of the real system clock. Every Service/Repository that needs
// "now" takes an IClock& (not a concrete clock, not a free function) so
// that tests can advance time deterministically without sleeping.
class IClock {
public:
    virtual ~IClock() = default;

    // Returns the current time point, using the same clock type
    // (std::chrono::system_clock) that persisted timestamps in JSON files
    // are serialized against, so time_point values read from disk and
    // values returned by this interface are directly comparable.
    virtual std::chrono::system_clock::time_point Now() const = 0;
};
```
Design notes to carry forward into later phases (record here since later phases will read this file's contract, not re-derive it):
- `system_clock` (not `steady_clock`) is the chosen clock type because production-queue completion times must be persisted to JSON and compared across process restarts — `steady_clock` has no fixed epoch and isn't meaningful once serialized to a file and read back by a later process invocation. This is a deliberate tradeoff (system_clock can jump on wall-clock adjustments) accepted because the spec has no requirement to be robust against system clock changes mid-run.
- `Now()` is `const` and takes no arguments — callers never construct time themselves, only ask "what time is it" through this seam.
- No `Sleep`/`WaitUntil` method is included — the spec's production queue is reconciled lazily at query time (per `CLAUDE.md`), never by blocking/waiting, so the clock abstraction only needs to answer "what time is it now," never "wait until X."

### 5. `SystemClock` — production implementation

`SampleOrderSystem/Core/SystemClock.h`:
```cpp
#pragma once
#include "IClock.h"

// Production IClock implementation backed by the real OS wall clock.
// Stateless; safe to construct as a single shared instance (or one per
// call site) since it holds no members.
class SystemClock final : public IClock {
public:
    std::chrono::system_clock::time_point Now() const override;
};
```
`SampleOrderSystem/Core/SystemClock.cpp`:
```cpp
#include "SystemClock.h"

std::chrono::system_clock::time_point SystemClock::Now() const {
    return std::chrono::system_clock::now();
}
```
This file is trivial by design — it exists only so production code always goes through the `IClock` seam, never calling `std::chrono::system_clock::now()` directly. (Worth a repo-wide convention note for later phases/reviewers: grep for direct `system_clock::now()` calls outside this file during later phase reviews as a smell check.)

### 6. `FakeClock` — test double

`SampleOrderSystemTests/FakeClock.h` (test-only; not compiled into the app, hence it lives under `SampleOrderSystemTests/` rather than `SampleOrderSystem/Core/`):
```cpp
#pragma once
#include "Core/IClock.h"

// Test double for IClock whose time is advanced explicitly by the test,
// never by real elapsed wall-clock time. Lets tests express "the
// production queue entry's completion time has now passed" without
// sleeping or depending on real-time timing flakiness.
class FakeClock final : public IClock {
public:
    // Starts at an arbitrary but fixed, deterministic epoch so tests don't
    // depend on "today" — chosen as 2024-01-01T00:00:00 UTC for readability
    // in failure messages. Tests that care about a specific start time
    // should call SetTime explicitly rather than relying on this default.
    FakeClock();
    explicit FakeClock(std::chrono::system_clock::time_point start);

    std::chrono::system_clock::time_point Now() const override;

    // Sets the clock to an absolute time point.
    void SetTime(std::chrono::system_clock::time_point time);

    // Advances the clock forward by the given duration. Passing a negative
    // duration is allowed (moves time backward) since nothing in this
    // class enforces monotonicity — tests that need "time never goes
    // backward" as a system invariant should test that invariant in the
    // component under test, not in the fake itself.
    void Advance(std::chrono::system_clock::duration delta);

private:
    std::chrono::system_clock::time_point m_time;
};
```
Inline implementation (header-only is fine given its small size and test-only scope; if preferred, a matching `FakeClock.cpp` added to the test project's `<ClCompile>` list is equally acceptable — this phase's author should pick whichever and note the choice, but header-only avoids one more vcxproj item):
```cpp
inline FakeClock::FakeClock()
    : m_time(std::chrono::sys_days{std::chrono::year{2024}/1/1}) {}

inline FakeClock::FakeClock(std::chrono::system_clock::time_point start)
    : m_time(start) {}

inline std::chrono::system_clock::time_point FakeClock::Now() const {
    return m_time;
}

inline void FakeClock::SetTime(std::chrono::system_clock::time_point time) {
    m_time = time;
}

inline void FakeClock::Advance(std::chrono::system_clock::duration delta) {
    m_time += delta;
}
```
(If `<chrono>`'s `sys_days`/calendar literals prove awkward with the toolset, an acceptable equivalent default is constructing from `std::chrono::system_clock::time_point{std::chrono::seconds{1704067200}}`, the Unix epoch offset for 2024-01-01T00:00:00 UTC — either is fine, just pick one and keep it deterministic.)

## Unit tests for this phase (in `SampleOrderSystemTests/ClockTests.cpp`)

1. **Smoke test** (above) — harness wiring, `[smoke]` tag.
2. **`SystemClock::Now()` returns a plausible current time** — tagged `[clock]`. Cannot assert exact equality against a fresh `system_clock::now()` call (inherently racy), so instead: call `SystemClock::Now()`, then `std::chrono::system_clock::now()` immediately after, and assert the two are within a generous tolerance (e.g. 1 second) of each other and that `SystemClock::Now()` is not equal to the default-constructed/epoch time_point (catches a broken stub that always returns `time_point{}`).
3. **`FakeClock` default-constructs to the fixed deterministic epoch** — assert `Now()` equals the documented 2024-01-01 start value exactly (not "close to" — this is a fake, so it must be exact).
4. **`FakeClock::SetTime` sets and `Now()` reflects it exactly** — set to an arbitrary time point, assert `Now()` returns exactly that value.
5. **`FakeClock::Advance` moves time forward by the given duration** — start at a known time, `Advance(std::chrono::hours(2))`, assert `Now()` equals start + 2 hours exactly. Also test advancing by a non-hour-aligned duration (e.g. `std::chrono::minutes(90)`) to catch truncation bugs.
6. **`FakeClock::Advance` with a negative duration moves time backward** — edge case per the design note above; assert it's allowed and exact, since later phases (e.g. testing "production not yet complete" boundary conditions) may rely on being able to rewind as well as advance.
7. **`FakeClock::Advance` called multiple times accumulates** — advance twice by different amounts, assert final time is start + sum of both deltas (catches an implementation that overwrites instead of accumulates).
8. **Boundary: advancing to exactly a target time point, not past it** — since later phases' production-queue-sweep logic will need "has the completion time passed" semantics (`now >= completionTime` most likely, confirm the exact operator against `docs/ARCHITECTURE.md`'s stated semantics once written and against `docs/REQUIREMENT.md` — this phase does not implement that comparison itself, just the clock that feeds it), add a test that sets `FakeClock` to exactly some time point `T` and confirms `Now() == T` precisely (not `>= T` by some fudge), since off-by-epsilon here would silently break every later phase's "exactly at the deadline" edge-case tests.

No tests are needed for `IClock` itself (it's a pure abstract interface with no behavior of its own to verify — its contract is exercised indirectly via `SystemClock` and `FakeClock`'s tests above, and directly by every later phase's service tests that take an `IClock&`).

## Interface/signature contract exposed to later phases

Later phases (Order/Sample/ProductionQueue services and repositories, per `docs/ARCHITECTURE.md`) must:
- `#include "Core/IClock.h"` and accept time via constructor-injected `const IClock&` (or `std::shared_ptr<IClock>`/reference member — the exact ownership pattern, e.g. whether services take clocks by reference or shared_ptr, should be fixed once in `docs/ARCHITECTURE.md`/phase-2's plan and used consistently; this phase only fixes the `IClock` contract itself, not callers' storage convention).
- Never call `std::chrono::system_clock::now()` directly outside `SystemClock.cpp`.
- In production `main()` wiring (app entry point, out of scope for this phase but relevant for phase sequencing), construct one `SystemClock` instance and pass it down to services.
- In tests, construct a `FakeClock`, pass it to the component under test, and call `SetTime`/`Advance` to move time forward to simulate production-queue completion deadlines passing, order timestamps advancing, etc., without any real sleeping.

## Residual risks / things not resolved by this phase

- Open Question #1 above is the primary blocker — must be signed off before implementation starts.
- Whether Win32 (in addition to x64) configurations are verified to build for the test project is left as a nice-to-have, not a blocking exit criterion, given the smoke test's purpose is proving the harness works at all.
- The exact `time_point` comparison operator (`>=` vs `>`) for "has a production queue entry's completion time passed" is explicitly deferred to the phase that implements the production queue sweep (not this phase) — this phase only guarantees the clock is precise enough (exact equality, no fudge) that whichever operator is chosen there behaves predictably at the boundary.
