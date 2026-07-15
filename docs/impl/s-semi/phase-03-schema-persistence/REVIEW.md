# pc-reviewer review: Phase 3 — Schema documents + persistence layer

Reviewed commit: `c341156`

Verified independently: full rebuild + run, 174/174 tests passed before fixes.

## Findings (ranked)

1. **[Critical, fixed] `schema/order.schema.json`'s `status` enum used the wrong casing
   (`"Reserved"`/`"Confirmed"`/...) and would have rejected every real `Order` record —
   `Order::ToJson()` (phase-4) serializes the canonical form all-caps (`"RESERVED"`, etc., per
   `Order.h`'s own documented convention). This would have broken 100% of the time as soon as any
   later phase persisted a real order. **Fix applied:** corrected the enum list to
   `["RESERVED","CONFIRMED","PRODUCING","RELEASED","REJECTED"]`; also fixed the same lower-cased
   fixture casing in `SchemaTests.cpp`/`SchemaValidatorTests.cpp` for consistency (those were
   internally self-consistent hand-built fixtures, not themselves broken, but misleadingly
   unrepresentative of the real files).
2. **[High, fixed] No test exercised the real checked-in `schema/*.schema.json` files against
   real domain-model `ToJson()` output** — exactly how finding #1 slipped through 174 passing
   tests. **Fix applied:** added three integration tests (`RealOrderSchemaFileAccepts...`,
   `RealSampleSchemaFileAccepts...`, `RealProductionQueueSchemaFileAccepts...`) that
   `LoadSchemaFromFile` the actual repo-checked-in schema files (resolved via `__FILE__`-relative
   path, stable regardless of the test binary's working directory) and validate a real model
   instance's `ToJson()` output against them.
3. **[Medium, fixed as documentation] ISO-8601 format check accepts calendar-impossible days
   (e.g. Feb 30)** — this is a deliberate, spec-sanctioned limitation (DETAIL.md explicitly scopes
   this validator to structural + coarse range checks, not full calendar correctness), but was
   untested either way. **Fix applied:** added an explicit test documenting the current lenient
   behavior, so a future tightening is a deliberate decision, not an accidental change.
4. **[No action] GoogleMock not needed for `JsonFileStoreTests.cpp`** — real temp-directory I/O
   is the correct approach since the class's entire contract (atomic rename, whole-file
   round-trip) is only meaningfully verified against a real filesystem. Confirmed appropriate,
   no change.

## Verdict

Mechanically careful implementation, but a real, guaranteed-to-break integration bug (finding #1)
slipped through purely because no test validated the real schema files against real model output.
All three real findings fixed; 178 tests pass after fixes (up from 174 — 4 new tests added).
