# pc-reviewer review: Phase 6 — ProductionService

Reviewed commit: `b4d0f62`

Verified independently: 262/262 tests passed. Cross-checked against `DETAIL.md`,
`docs/ARCHITECTURE.md`. This review also covers the verify job (no separate per-phase Verify step).

## Findings (ranked)

1. **[Critical, fixed] `ProductionService.cpp` broke the real app build.** Line 1 was
   `#include "Services/ProductionService.h"` instead of the repo's established
   self-include-by-bare-filename convention (`#include "ProductionService.h"`). This only compiled
   in `SampleOrderSystemTests.vcxproj` by accident, because that project's
   `AdditionalIncludeDirectories` happens to include `..\SampleOrderSystem`; the actual console app
   project (`SampleOrderSystem.vcxproj`) has no such override and failed with
   `C1083: cannot open include file 'Services/ProductionService.h'`. The "262 tests pass" claim was
   true but did not verify the app itself still builds. **Fix applied:** corrected the include to
   `"ProductionService.h"`. This also surfaced the same wrong-style includes in
   `ProductionService.h` itself (`"Core/IClock.h"`, `"Models/ProductionQueueEntry.h"`,
   `"Repositories/*.h"`), which only worked for the same accidental reason — **fixed** to
   `"../Core/IClock.h"` etc. to match the convention already used by `Repositories/SampleRepository.h`
   (`"../Models/Sample.h"`, `"../Persistence/JsonFileStore.h"`). Confirmed fix by rebuilding
   `SampleOrderSystem.vcxproj` directly: compiles cleanly now (only the pre-existing, out-of-scope
   `main()` link error remains, expected until the main-wiring phase).
2. **[Low, documented] `SettleDueEntries` performs 3 non-atomic file writes per due entry**
   (stock credit -> order status flip -> queue removal). A crash between them could double-credit
   stock on the next sweep (entry still queued -> reprocessed), never silently lose it — judged the
   safer of the two failure modes, but was undocumented. **Fix applied:** added a one-line comment
   in `ProductionService.cpp` acknowledging the trade-off.
3. **[Nitpick, no action] No test for the "order found but not Producing" defensive branch** —
   DETAIL.md itself calls this "a defensive branch, not a designed-for path," so left as-is per the
   explicit request to keep the test set lean.

## Verdict

Implementation logic, math, and FIFO/lazy-settlement behavior all correctly match `DETAIL.md` and
`ARCHITECTURE.md`, and the test suite is thorough for the domain logic — but the include-path bug
was a real, release-blocking defect that "tests are green" alone didn't catch, which is exactly why
this review step also does the verify job now.
