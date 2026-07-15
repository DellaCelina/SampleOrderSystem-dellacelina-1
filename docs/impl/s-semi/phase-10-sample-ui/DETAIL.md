# Phase 10: Sample UI (SampleView + SampleController)

**Depends on:** Phase 5 (repositories)
**Touches:** `SampleOrderSystem/Views/SampleView.h`, `SampleOrderSystem/Views/SampleView.cpp`, `SampleOrderSystem/Controllers/SampleController.h`, `SampleOrderSystem/Controllers/SampleController.cpp`

## Summary

Implement the console rendering (SampleView, no file I/O) and input-handling/orchestration (SampleController, no direct JSON access) for registering samples (rejecting duplicate sample IDs without mutating the existing record) and listing/searching samples by exact ID or case-insensitive name substring, calling SampleRepository directly. Depends only on phase-5's SampleRepository; independent of Order/Production/Monitoring services and their UI, so it can be built in parallel with phases 6-9 and with phases 11-13.

**Namespace/test-framework correction:** everything is in the **global namespace** (no
`Views::`/`Models::`/`Repositories::` wrappers), matching phase-1/2/4/5's real committed code.
Tests are GoogleTest, not Catch2 — see phase-1's DETAIL.md superseding note.

## Detail

## Behavior to implement

**SampleView** (`SampleOrderSystem/Views/SampleView.h/.cpp`) — pure console rendering plus raw line-reading; no file I/O, no business validation, no calls to any repository/service. To make it unit-testable without capturing the real console, it is constructed with injectable streams:

```cpp
namespace Views {
class SampleView {
public:
    explicit SampleView(std::istream& in = std::cin, std::ostream& out = std::cout);

    // Output
    void ShowSampleList(const std::vector<Sample>& samples) const;   // used for both "list all" and search results
    void ShowNoSamples() const;                                             // printed by ShowSampleList when the vector is empty
    void ShowRegistrationSuccess(const Sample& sample) const;
    void ShowError(const std::string& message) const;

    // Input (raw only — no parsing/validation here, that's the Controller's job)
    std::string PromptLine(const std::string& promptText) const;            // writes promptText to out_, reads one line from in_, trims leading/trailing whitespace
private:
    std::istream& in_;
    std::ostream& out_;
};
}
```

Design choices baked in here, so the TDD implementer doesn't have to re-derive them:
- `ShowSampleList` is the single rendering routine for both the "list all samples" screen and "search results" screen — same five columns (sample ID, name, average production time (min), yield, current stock), one row per sample, in the order the input vector is given (View does not sort). When given an empty vector it calls/prints the same "no samples" content as `ShowNoSamples` (implement `ShowSampleList` by delegating to `ShowNoSamples` when `samples.empty()`, rather than duplicating the empty-case string in two places).
- `PromptLine` is the *only* input primitive. It does not parse ints/doubles and does not loop on bad input — that responsibility belongs entirely to `SampleController`, keeping View "dumb" (no business logic, satisfies "View performs no direct file I/O" and keeps it trivially testable with `std::istringstream`/`std::ostringstream`).
- Exact console copy (English vs Korean, column widths, etc.) is unspecified by `docs/ARCHITECTURE.md` (see its Open Questions — "language-agnostic, Views own the text, decided per-phase"). Pick any consistent, readable format for this phase; tests should assert on the presence/content of the relevant *data* (sample id, name, numbers) in the output, not on exact byte-for-byte formatting/whitespace, so later phases' Views can pick their own compatible style without this phase's tests becoming brittle.

**SampleController** (`SampleOrderSystem/Controllers/SampleController.h/.cpp`) — reads input via the `SampleView` it's given, validates/parses, calls `SampleRepository` directly (per this phase's own description — no repository interface/abstraction layer), and routes results back through the `SampleView`. No direct JSON/file access (that all lives inside `SampleRepository`).

```cpp
namespace Controllers {
class SampleController {
public:
    SampleController(SampleRepository& repository, SampleView& view);

    void HandleRegister();   // register a new sample
    void HandleListAll();    // list every registered sample
    void HandleSearch();     // search by exact ID or case-insensitive name substring

private:
    SampleRepository& repository_;
    SampleView& view_;
};
}
```

### Assumed `SampleRepository` surface (from phase-5, per `docs/ARCHITECTURE.md`'s Repositories section)

Phase-5 is described as: `data/samples.json`; CRUD + `FindById` (exact) + `FindByNameSubstring` (case-insensitive) + stock mutation helpers. This phase needs exactly this subset and assumes these signatures; if phase-5's actual implementation differs in shape (e.g. `Add` returns a `Result`/throws instead of `bool`, or `FindById` returns a pointer instead of `optional`), adapt `SampleController`'s calls to match phase-5's real API — the important, load-bearing contract this phase depends on is: **`Add` on a duplicate `sampleId` fails without mutating the existing stored record**, `FindById` does exact-match lookup, `FindByNameSubstring` does case-insensitive substring matching, and there is some "get everything" accessor for the list screen.

```cpp
namespace Repositories {
class SampleRepository {
public:
    bool Add(const Sample& sample);                                    // false + no mutation if sampleId already exists
    std::optional<Sample> FindById(const std::string& sampleId) const; // exact match
    std::vector<Sample> FindByNameSubstring(const std::string& text) const; // case-insensitive substring
    std::vector<Sample> GetAll() const;
    // stock mutation helpers exist for Order/Production services — NOT used by this phase
};
}
```

`Sample` fields (from `docs/ARCHITECTURE.md`'s Domain models section): `sampleId` (string), `name` (string), `averageProductionTimeMinutes` (int), `yield` (double, `0 < yield <= 1`), `currentStock` (int).

### `HandleRegister` flow and validation rules

1. Prompt (via `view_.PromptLine`) in order: sample ID, name, average production time (minutes), yield.
2. Validate, in this order, aborting on the first failure (no partial repository writes, no retry loop — this phase does single-pass validation, not an interactive re-prompt loop; the requirement only mandates rejection-without-mutation, not a retry UX):
   - sample ID non-empty after trim → else `view_.ShowError("Sample ID must not be empty")` (or equivalent) and return.
   - name non-empty after trim → else error and return.
   - average production time parses as an integer and is `> 0` → else error and return. (Non-numeric input and `<= 0` are both rejected; this phase does not need to distinguish "not a number" from "zero or negative" in the message text, just report *some* validation error and abort.)
   - yield parses as a floating point number and satisfies `0 < yield <= 1` → else error and return.
3. Construct `Sample{ sampleId, name, averageProductionTimeMinutes, yield, currentStock = 0 }` — **registration always sets `currentStock` to 0**; there is no "initial stock" prompt, since neither the requirement's scope bullet nor its acceptance criteria for registration mention an initial-stock input (stock only ever changes via production completion / release, which are out of this phase's scope).
4. Call `repository_.Add(sample)`.
   - If it returns `true`: `view_.ShowRegistrationSuccess(sample)`.
   - If it returns `false` (duplicate ID): `view_.ShowError(...)` naming the duplicate id; the existing stored record must be provably untouched (this is phase-5's contract, but this phase's tests should still assert it black-box, since it's the acceptance-criterion behavior actually being exercised end-to-end here).

### `HandleListAll` flow
Calls `repository_.GetAll()` and passes the result straight to `view_.ShowSampleList(...)` — no filtering, no sorting, no settlement call. **This phase must not call anything from `ProductionService`/`OrderService`/lazy settlement** — those don't exist in this phase's dependency set (only phase-5's `SampleRepository`), and `docs/ARCHITECTURE.md`'s settlement obligation attaches to Order/Monitoring/ProductionLine query paths, not the plain sample list/search screens. The sample list simply reflects whatever `currentStock` is currently stored.

### `HandleSearch` flow
1. Prompt for search mode via `view_.PromptLine` (e.g. "1) By ID  2) By name substring — choose: ") and parse the response.
   - If it's not recognizable as `"1"` or `"2"` (or your chosen valid tokens): `view_.ShowError("Invalid search option")` and return — do not call the repository.
2. Prompt for the search term via `view_.PromptLine`.
3. If mode is "by ID": call `repository_.FindById(term)`; wrap the `optional<Sample>` result into a 0- or 1-element `vector<Sample>` and pass to `view_.ShowSampleList(...)` (reusing the same rendering path as list-all keeps View simple and gives search results and list-all identical column output, satisfying the requirement that both screens show the same 5 fields).
4. If mode is "by name": call `repository_.FindByNameSubstring(term)` and pass the resulting vector to `view_.ShowSampleList(...)` directly.

## Unit tests to write

### `SampleView` tests (construct with `std::istringstream`/`std::ostringstream`, no real console)
1. `ShowSampleList` with an empty vector — output contains the "no samples" message (and only that; no stray table header).
2. `ShowSampleList` with one sample — output contains that sample's id, name, average time, yield, and stock.
3. `ShowSampleList` with multiple samples — output contains all of them, one row each, in the given (input) order — do not assert any particular sort order since the View must not impose one.
4. `ShowRegistrationSuccess` — output contains the registered sample's id and name.
5. `ShowError` — output contains the exact message text passed in (so Controller-level tests can assert on distinguishable error text per validation rule).
6. `PromptLine` — writes the given prompt text to the output stream, then reads exactly one line from the input stream and returns it with leading/trailing whitespace trimmed (test: input `"  ABC  \n"` → returns `"ABC"`).
7. `PromptLine` called twice in sequence on a stream pre-loaded with two lines — returns the two lines in order (simulates a multi-field registration prompt sequence in one test).
8. `PromptLine` on an empty line — returns an empty string (does not throw/crash).

### `SampleController` tests
Since this phase couples directly to the concrete `SampleRepository` (no interface/mock), tests construct a **real** `SampleRepository` backed by a temporary/test-scoped `data/samples.json` path (create in a fresh temp directory per test, clean up after), and a real `SampleView` wired to `std::istringstream`/`std::ostringstream` fed with scripted input.

`HandleRegister`:
1. Valid input (unique id, non-empty name, `"10"`, `"0.9"`) → after the call, `repository_.FindById(id)` returns a sample with exactly those field values and `currentStock == 0`; view output contains a success message.
2. Duplicate sample ID — pre-seed the repository with an existing sample for `id`, then run `HandleRegister` with the same `id` but different name/time/yield values → `repository_.FindById(id)` still returns the **original seeded values unchanged**; `repository_.GetAll().size()` unchanged; view output contains an error message (and not the success message).
3. Non-numeric average production time (e.g. `"abc"`) → `repository_.FindById(id)` returns `nullopt` (no record created); view shows a validation error.
4. Average production time `<= 0` (e.g. `"0"`, `"-5"`) → rejected, no record created, error shown.
5. Non-numeric yield (e.g. `"abc"`) → rejected, no record created, error shown.
6. Yield out of range — table-test `"0"`, `"-0.2"`, `"1.5"` → each rejected, no record created, error shown.
7. Yield boundary `"1"` (i.e. exactly 1.0) → **accepted** (valid boundary per `0 < yield <= 1`).
8. Empty sample ID (blank/whitespace-only line) → rejected, no record created, error shown. (This blank-rejection rule is an inferred minimal rule, not stated verbatim in the requirement — flagging it here so it's a documented decision rather than a silent one; a blank ID would otherwise be unsearchable/ambiguous.)
9. Empty name → same as above.

`HandleListAll`:
1. Empty repository → view output is the "no samples" content (verifies the controller passes `GetAll()`'s empty result straight through rather than special-casing it itself — the empty-case handling lives in `SampleView`).
2. Repository seeded (directly via `repository_.Add(...)` in test setup, not through the controller) with N samples → `HandleListAll()` output contains all N samples' ids/names/stocks.

`HandleSearch`:
1. Mode "by ID", term = an existing id → output contains exactly that one sample's row.
2. Mode "by ID", term = a non-existent id → output is the "no samples"/empty message.
3. Mode "by name", term matches multiple samples case-insensitively (e.g. seeded names `"Widget"`, `"widget-2"`, `"Gadget"`, searching `"widget"`) → output contains both `"Widget"` and `"widget-2"` rows, not `"Gadget"`.
4. Mode "by name", term matches nothing → "no samples" message.
5. Invalid mode input (e.g. `"3"` or `"x"`) → view output contains an error message and no sample-list/table content at all; the repository is not queried for this case (this can be asserted purely from output content, since the invalid-mode path never reaches a `ShowSampleList` call).

## Explicitly out of scope for this phase
- Any interaction with `OrderService`, `ProductionService`, `MonitoringService`, `ProductionLineViewService`, or lazy settlement — none of those are dependencies of phase-10, and the sample list/search screens do not trigger or depend on settlement.
- Stock mutation — this phase never calls any stock-mutation helper on `SampleRepository`; new samples always start at `currentStock = 0`.
- Any menu wiring into `MainMenuController` (that's a separate phase); this phase only needs `SampleController`'s three public methods to exist with the signatures above so a later phase can wire them into the menu loop.
- Exact console string wording/formatting — left to the implementer; only the underlying data-presence assertions above are binding for tests, per the Open Question in `docs/ARCHITECTURE.md` noting UI copy is undecided and per-phase.

## Files touched
- `SampleOrderSystem/Views/SampleView.h`, `SampleOrderSystem/Views/SampleView.cpp` — new.
- `SampleOrderSystem/Controllers/SampleController.h`, `SampleOrderSystem/Controllers/SampleController.cpp` — new.
- Test files for these (wherever `SampleOrderSystemTests` conventions land, e.g. `SampleOrderSystemTests/Views/SampleViewTests.cpp`, `SampleOrderSystemTests/Controllers/SampleControllerTests.cpp`) are implied but not explicitly listed in this phase's `touches`; per `docs/ARCHITECTURE.md`'s Open Questions, whether Controllers/Views get automated test coverage at all was left open — this phase plan resolves that open question **for itself** by requiring the tests above, since TDD is the working method for every phase per the project's process.

## Dependency/parallelism note
This phase only requires phase-5's `SampleRepository` to exist and compile with (at least) the four methods described above. It does not read or write `Order`, `ProductionQueueEntry`, or any Order/Production/Monitoring service, and it doesn't touch `MainMenuController` or any other Views/Controllers file — so it is safe to implement in parallel with phases 6–9 (Order/Production/Monitoring services) and phases 11–13 (their UI), as stated in the phase summary. If phase-5's actual `SampleRepository` signatures differ from the assumed ones above, this phase's Controller code (not its test *intent*) should be adjusted to match — but that adjustment should not require changes to `SampleView` at all, since `SampleView` has zero repository/service dependencies.
