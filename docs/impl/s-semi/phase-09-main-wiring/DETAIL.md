# Phase 9 (consolidated, formerly Phase 14): MainMenuController + main.cpp wiring and CLI flags

**Depends on:** Phase 10 (sample-ui), Phase 8 (consolidated UI: order-ui, monitoring-production-ui, data-monitor-dummy-ui)
**Touches:** `SampleOrderSystem/Controllers/MainMenuController.h`, `SampleOrderSystem/Controllers/MainMenuController.cpp`, `SampleOrderSystem/main.cpp`

## Summary

Wire everything together: main.cpp constructs a SystemClock and all repositories/services, then either enters MainMenuController's interactive loop exposing every feature (sample management, order submission/approval/rejection/release, monitoring, production-line view, data monitor, dummy data) as menu items, or — if invoked with a CLI flag such as --dummy-data or --data-monitor — runs that mode directly and exits without the interactive loop. Also pins down where data/ and schema/ are resolved relative to (executable path, per the Open Question) so the shipped binary finds its JSON files regardless of launch directory. This phase necessarily depends on all four UI-area phases (10-13) since MainMenuController's menu construction touches all of them, and is the last integration point before the whole console app is runnable end to end.

## Detail

## Assumptions carried in from phases 10-13 (state these explicitly if they turn out wrong)

This phase's own `touches` list is only `MainMenuController.h/.cpp` and `main.cpp`, so it cannot invent new controller/service signatures — it can only *consume* what phases 10-13 built. Since those phases' own IMPLEMENT.md detail isn't available to this writer, the following interfaces are assumed based on the Controllers list in `docs/ARCHITECTURE.md` ("Console MVC layer" section) and must be reconciled against whatever those phases actually shipped before this phase's code compiles:

- `SampleController` (phase-10): has methods callable from a menu loop, e.g. `void RegisterSample()`, `void SearchSamples()` — each reads whatever input it needs from `std::cin` itself and renders via `SampleView`.
- `OrderController` (phase-11): `void SubmitOrder()`, `void ListPendingApprovals()`, `void ApproveOrder()`, `void RejectOrder()`, `void ReleaseOrder()`.
- `MonitoringController` (phase-12): `void ShowStatusSummary()` (wraps `MonitoringService`) and `void ShowProductionLine()` (wraps `ProductionLineViewService`) — assumed to live on one controller since the architecture lists only one `MonitoringController`, not a separate `ProductionLineViewController`, even though the two backing services are separate.
- `DataMonitorController` (phase-13): `void ShowDataMonitor()`.
- `DummyDataController` (phase-13): `void GenerateDummyData(int count)` (or similar) — wraps `DummyDataGenerator`.

Each of these controllers is assumed to already hold (by constructor injection) whatever repositories/services/`IClock&` it needs, per Key Design Decision #4 (clock injected, never a singleton). `MainMenuController` does not need its own `IClock&` — it never calls a service directly, only delegates to the above controllers.

If any of phases 10-13 named these methods differently, grouped production-line view under a different controller, or gave controllers a different construction pattern (e.g. returning data to a caller-owned View instead of rendering internally), adjust the dispatch table below accordingly — the *shape* of this phase (a flat menu → controller-method dispatch table, plus the CLI-flag short-circuit in `main.cpp`) does not change.

## Additional file this phase must also touch, beyond the stated list

`SampleOrderSystem/SampleOrderSystem.vcxproj` needs a post-build step added (or the project's working-directory/output settings need it) so that `data/` and `schema/` end up next to the produced `.exe` regardless of build configuration (`x64/Debug/`, `x64/Release/`, etc.) — see path resolution below. Flag this explicitly to the implementer rather than silently editing the vcxproj outside the phase's declared touches; if the caller wants to keep `touches` minimal, this line item should be called out as a required amendment before merging this phase.

## Path resolution (resolves ARCHITECTURE.md's Open Question: data/schema relative to exe vs cwd)

Decision: **relative to the executable's own directory, not the process's current working directory.** This is what lets the shipped binary be launched by double-click, from Visual Studio's debugger, or from an arbitrary shell cwd and still find its files.

In `main.cpp`, add:
```cpp
// Windows-only; project is a Win32/x64 VS console app per CLAUDE.md, so no cross-platform fallback needed.
std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        throw std::runtime_error("Failed to resolve executable path");
    }
    return std::filesystem::path(buffer).parent_path();
}
```
`main.cpp` then computes `dataDir = ExecutableDirectory() / "data"` and `schemaDir = ExecutableDirectory() / "schema"`, and passes these `std::filesystem::path`s into the repository/schema-loading constructors built in earlier phases (repositories are assumed, per ARCHITECTURE.md's Repositories section, to take a base path or explicit file path — wire whichever signature they actually exposed). The post-build step above must copy the repo-root `data/` and `schema/` folders into `$(OutDir)` so this resolution succeeds for both Debug and Release, both Win32 and x64.

If `dataDir` does not exist at startup, **create it** (`std::filesystem::create_directories`) — an empty/missing `data/` directory is a legitimate first-run state (no samples/orders/queue yet), distinct from a *malformed* file, which per Key Design Decision/Persistence section must hard-fail. If `schemaDir` is missing, that is a packaging/deployment error (schemas are committed, shipped, required) — `main.cpp` reports this to console and exits non-zero without entering any mode.

## CLI flag grammar

`main.cpp` parses `argv[1..]` before constructing anything expensive (clock/repositories are still constructed first, since even flag modes need them):

- No flags → interactive mode (enter `MainMenuController::Run()`'s loop).
- `--dummy-data` or `--dummy-data=<N>` → run dummy data generation directly (`N` defaults to a fixed constant, e.g. 20, if omitted or non-numeric) then exit 0. Does not enter the interactive loop.
- `--data-monitor` → render the data monitor view once (after the same lazy-settlement sweep the interactive menu item would trigger) then exit 0. Does not enter the interactive loop.
- `--help` / `-h` → print a one-line usage summary (`SampleOrderSystem.exe [--dummy-data[=N]] [--data-monitor]`) to stdout, exit 0, no repositories touched.
- Unknown flag, or more than one of `--dummy-data`/`--data-monitor` given together → print an error to stderr naming the offending argument(s), exit 1, no mode entered (this is a "reject malformed input, don't guess" rule mirroring the JSON-loading philosophy elsewhere in the architecture).
- A repository/schema load failure at startup (malformed JSON, failed schema validation) is caught in `main.cpp`, reported to console with the file name and reason (per Persistence section's "whole-table, fail-fast" rule), and the process exits non-zero *before* either the flag branch or the interactive loop runs — this applies uniformly regardless of which mode was requested.

Implement flag parsing as a small **free, pure function** so it is unit-testable despite living in files the test project doesn't currently compile (see Testing strategy below):
```cpp
enum class CliMode { Interactive, DummyData, DataMonitor, Help, Error };
struct CliArgs {
    CliMode mode = CliMode::Interactive;
    int dummyDataCount = 20;       // only meaningful when mode == DummyData
    std::string errorMessage;      // only meaningful when mode == Error
};
CliArgs ParseCliArgs(int argc, char* const argv[]);
```
Declare and define this as a free function (not a member) in `MainMenuController.h`/`.cpp`, taking no dependency on any controller/service/repository — pure string-in, struct-out.

## `main.cpp` control flow (pseudocode, exact order matters)

```cpp
int main(int argc, char** argv) {
    CliArgs args = ParseCliArgs(argc, argv);
    if (args.mode == CliMode::Help) { PrintUsage(); return 0; }
    if (args.mode == CliMode::Error) { std::cerr << args.errorMessage; return 1; }

    auto exeDir = ExecutableDirectory();
    auto dataDir = exeDir / "data";
    auto schemaDir = exeDir / "schema";
    std::filesystem::create_directories(dataDir);

    try {
        SystemClock clock;
        // construct repositories, then services (injecting clock where required per Key Design Decision #4),
        // then Views, then Controllers, per the dependency direction in ARCHITECTURE.md's Components list.
        SampleRepository sampleRepo(dataDir / "samples.json", schemaDir / "sample.schema.json");
        OrderRepository orderRepo(dataDir / "orders.json", schemaDir / "order.schema.json");
        ProductionQueueRepository queueRepo(dataDir / "production_queue.json", schemaDir / "production_queue.schema.json");
        ProductionService productionService(queueRepo, sampleRepo, orderRepo);
        OrderService orderService(orderRepo, sampleRepo, productionService);
        MonitoringService monitoringService(sampleRepo, orderRepo, productionService);
        ProductionLineViewService lineViewService(queueRepo, productionService);
        DummyDataGenerator dummyGen(sampleRepo, orderRepo, queueRepo, clock);

        SampleView sampleView; OrderView orderView; MonitoringView monitoringView;
        ProductionLineView lineView; DataMonitorView dataMonitorView;

        SampleController sampleController(sampleRepo, sampleView);
        OrderController orderController(orderService, orderView);
        MonitoringController monitoringController(monitoringService, lineViewService, monitoringView, lineView, clock);
        DataMonitorController dataMonitorController(sampleRepo, orderRepo, queueRepo, productionService, dataMonitorView, clock);
        DummyDataController dummyDataController(dummyGen);

        switch (args.mode) {
            case CliMode::DummyData:
                dummyDataController.GenerateDummyData(args.dummyDataCount);
                return 0;
            case CliMode::DataMonitor:
                dataMonitorController.ShowDataMonitor();
                return 0;
            default: {
                MainMenuController menu(sampleController, orderController, monitoringController,
                                         dataMonitorController, dummyDataController);
                return menu.Run();
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }
}
```
Adjust constructor arguments to match the actual signatures phases 10-13 produced; the ordering constraint that must be preserved regardless is: **repositories → services (clock injected) → views → controllers → mode dispatch**, and the flag-mode branches must reuse the exact same controllers the interactive menu would use (no separate code path for "flag mode" logic — this is what keeps the CLI-flag and menu-item routes to dummy-data/data-monitor behaviorally identical, per Key Design Decision #9).

## `MainMenuController` class shape

```cpp
class MainMenuController {
public:
    MainMenuController(SampleController& sampleController,
                        OrderController& orderController,
                        MonitoringController& monitoringController,
                        DataMonitorController& dataMonitorController,
                        DummyDataController& dummyDataController);

    // Runs the interactive loop until the user exits or input is exhausted (EOF).
    // Returns a process exit code (0 on normal exit).
    int Run();

private:
    void PrintMenu() const;
    // Returns std::nullopt on EOF/unreadable input (caller treats this as "exit").
    std::optional<int> ReadMenuChoice() const;
    void Dispatch(int choice); // no-op + "invalid choice" message for unmapped ints
};
```

Menu items and their dispatch targets (numbering is this phase's own choice; keep it stable once implemented since it's user-facing, not an API other phases depend on):

```
1. Register sample                 -> sampleController.RegisterSample()
2. Search samples                  -> sampleController.SearchSamples()
3. Submit order                    -> orderController.SubmitOrder()
4. List pending approvals          -> orderController.ListPendingApprovals()
5. Approve order                   -> orderController.ApproveOrder()
6. Reject order                    -> orderController.RejectOrder()
7. Release order                   -> orderController.ReleaseOrder()
8. Show monitoring summary         -> monitoringController.ShowStatusSummary()
9. Show production line            -> monitoringController.ShowProductionLine()
10. Data monitor                   -> dataMonitorController.ShowDataMonitor()
11. Generate dummy data            -> dummyDataController.GenerateDummyData(<prompted count>)
0. Exit                            -> breaks the loop, Run() returns 0
```

`Run()`'s loop: print menu, read a line, attempt `int` parse (reject non-numeric input with an error message and re-prompt, don't crash/throw), dispatch. If `std::cin` reaches EOF or an unrecoverable fail state (e.g. input piped from a script that runs out of lines), `ReadMenuChoice()` returns `std::nullopt` and `Run()` exits the loop and returns 0 — this must not spin in a busy loop re-reading a failed stream. Every dispatched controller call is wrapped so that an exception thrown *during* an operation (e.g. approving a nonexistent order number, a corrupt sample lookup) is caught at the `MainMenuController::Dispatch` level, printed as an error, and the loop continues to the next menu display rather than propagating out of `Run()` — only startup-time repository/schema load failures in `main.cpp` are fatal; once inside the interactive loop, per-operation errors are recoverable and should not kill the whole session.

## Testing strategy (this phase must explicitly resolve ARCHITECTURE.md's Open Question #4 for itself)

Per the architecture's "Build/test wiring" section, `SampleOrderSystemTests.vcxproj` only compiles `Core/`, `Json/`, `Persistence/`, `Models/`, `Repositories/`, `Services/` — explicitly **not** `Controllers/`, `Views/`, or `main.cpp`. This phase does not overturn that; `MainMenuController`'s interactive loop and `main.cpp`'s wiring get **manual/smoke verification only**, not GoogleTest coverage, for this iteration. State this explicitly rather than silently having zero tests for the phase:

1. **Unit-testable via GoogleTest (requires adding `MainMenuController.h` — header only, not `.cpp` — to `SampleOrderSystemTests.vcxproj`'s compiled/included sources, since `ParseCliArgs` is a free function with zero dependency on Controllers/Views/repositories):**
   - `ParseCliArgs` with no args → `CliMode::Interactive`.
   - `ParseCliArgs` with `--dummy-data` → `CliMode::DummyData`, `dummyDataCount == 20` (default).
   - `ParseCliArgs` with `--dummy-data=50` → `CliMode::DummyData`, `dummyDataCount == 50`.
   - `ParseCliArgs` with `--dummy-data=abc` (non-numeric) → falls back to the default count (20), does not throw.
   - `ParseCliArgs` with `--data-monitor` → `CliMode::DataMonitor`.
   - `ParseCliArgs` with `--help` and `-h` → `CliMode::Help`.
   - `ParseCliArgs` with both `--dummy-data` and `--data-monitor` together → `CliMode::Error` with a non-empty `errorMessage`.
   - `ParseCliArgs` with an unrecognized flag (e.g. `--bogus`) → `CliMode::Error`.
   - `ParseCliArgs` with a flag plus unrelated positional garbage → decide and test one consistent behavior (recommended: treat unexpected positional args the same as an unknown flag → `CliMode::Error`, since the requirement doesn't call for positional args at all).

2. **Not unit-testable this iteration (manual verification checklist to include in the phase's own notes/PR description, since it isn't GoogleTest coverage):**
   - Launching the built `.exe` from a directory other than its own (e.g. via `cd C:\; .\path\to\SampleOrderSystem.exe`) still finds `data/`/`schema/` correctly — proves the exe-relative path resolution.
   - `--dummy-data` and `--data-monitor` run their mode and exit without ever printing the interactive menu.
   - Each of the 11 interactive menu items dispatches to the right controller (spot-check a couple, e.g. option 5 approve vs option 6 reject, since these are easy to transpose).
   - Invalid menu input (non-numeric, or a number outside 0-11) prints an error and redisplays the menu rather than crashing.
   - Piped/redirected input that runs out (EOF) exits cleanly (exit code 0) instead of looping forever.
   - A deliberately corrupted `data/samples.json` (bad JSON) causes `main.cpp` to print a clear error and exit non-zero, for every mode (interactive, `--dummy-data`, `--data-monitor`) — confirms the fail-fast load behavior is wired through this integration point, not just unit-tested in isolation at the repository level.

## Interfaces this phase exposes (for completeness — nothing currently depends on phase-14, it is a leaf/integration phase)

- `CliArgs ParseCliArgs(int argc, char* const argv[])` and the `CliMode` enum, declared in `MainMenuController.h` — reusable if a later iteration adds a scripting/batch mode.
- `MainMenuController(SampleController&, OrderController&, MonitoringController&, DataMonitorController&, DummyDataController&)` constructor and `int Run()` — the single call site `main.cpp` needs; no other phase is expected to call into `MainMenuController` itself.
- `std::filesystem::path ExecutableDirectory()` — if useful elsewhere (e.g. a future logging phase), this is a small enough pure utility that it could be promoted into `Core/` later, but for this phase it stays local to `main.cpp` since nothing else currently needs it.

## Edge cases to explicitly cover in code (not just tests)

- Empty `data/` directory on first run (no `samples.json`/`orders.json`/`production_queue.json` yet) must not be treated as a load failure — repositories from earlier phases are assumed to treat "file does not exist" as "empty table," distinct from "file exists but is malformed" (which is fatal per Persistence section). `main.cpp`'s `create_directories` call only ensures the directory exists; it must not create empty/placeholder JSON files itself, since that's the repositories' responsibility and this phase shouldn't duplicate it.
- `--dummy-data=0` — a count of zero is a valid (if useless) request; don't special-case it as an error, just generate nothing and exit 0.
- Negative or absurdly large `--dummy-data=<N>` — clamp or reject with a clear message rather than passing straight through to the generator (recommend: reject negative values as a CLI parse error, same bucket as non-numeric).
