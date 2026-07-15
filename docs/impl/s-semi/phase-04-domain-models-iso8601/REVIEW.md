# pc-reviewer review: Phase 4 + GoogleTest/GoogleMock migration

Reviewed commits: `f8da8fc` (test framework migration), `08b455f` (Phase 4 implementation)

Verified independently: full rebuild + run with `TZ=Asia/Tokyo` to confirm Iso8601's
timezone-independence; assertion counts across every converted Catch2→GoogleTest file matched
exactly (69/69, 59/59, 17/17, 13/13); vcxproj/filters wiring for `TestMain.cpp`, `Core/Iso8601.*`,
`Models/*` confirmed correct in both project files.

## Findings (ranked)

1. **[Real bug, fixed] `FromJson` on all three models gave a misleading error for a non-object
   top-level `JsonValue`.** `RequireField`'s `Has()` check returns `false` (not throwing) for a
   non-object value, so `Sample::FromJson(JsonValue::MakeArray())` threw "missing required field
   sampleId" instead of the real problem. Matters most for `ProductionQueueEntry::FromJson`, which
   DETAIL.md calls out as the most plausible real-world malformed-record case. **Fix applied:**
   added an explicit `if (!json.IsObject()) throw std::invalid_argument(...)` guard at the top of
   all three `FromJson` methods, plus a test case per model.
2. **[Informational, no fix needed now]** No GoogleMock usage yet — `FakeClock` remains a
   hand-rolled fake, which is appropriate for a simple value type. Flagged for later phases:
   `ProductionService`/`OrderService` will need to mock repositories for interaction-style tests;
   those are the natural first `MOCK_METHOD` candidates.
3. **[Nitpick, fixed] `OrderTest`'s missing-required-field test used one loop-based `TEST` instead
   of five named tests**, unlike `SampleTest`'s equivalent (five separate `TEST`s) — inconsistent
   style and weaker failure diagnostics (a loop failure doesn't name which field). **Fix applied:**
   split into five named `TEST`s matching `Sample`'s pattern.

## Verdict

Solid migration and implementation. One real (if low-severity) validation gap found and fixed;
one style inconsistency fixed; one informational note for future phases, no action needed now.
122 tests pass after fixes (up from 115 — 4 new non-object tests + net +3 from unrolling the
`Order` loop into 5 named tests instead of 1).
