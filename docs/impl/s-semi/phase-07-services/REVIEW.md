# pc-reviewer review: Phase 7 — Remaining services

Reviewed commit: `0f9deb8`

Verified independently: 291/291 tests passed (at time of review). Cross-checked against all three
DETAIL docs, `docs/REQUIREMENT.md`, `docs/ARCHITECTURE.md`. This review also covers the verify job
(no separate per-phase Verify step).

## Findings (ranked)

1. **[Medium, fixed — and surfaced a real bug] `DummyDataGenerator` test coverage was thin relative
   to its own 20-test DETAIL spec.** Two gaps flagged as load-bearing: FIFO completion-time chaining
   across multiple Producing entries generated in the same call (test #11), and `runningClaims_`
   seeding from pre-existing Confirmed/Producing orders at construction (test #17). **Fix applied:**
   added both regression tests. Writing the FIFO-chaining test (with a smaller 3-sample/10-order
   scenario than the existing 10-sample/25-order determinism test) **surfaced a genuine, previously
   latent bug**: `TopUpStockIfNeeded` computed its top-up delta from `UnclaimedStock()`'s clamped
   (floor-0) value rather than the true, possibly deeply-negative deficit. When a sample's running
   claims already exceeded its stock by more than the requested top-up amount, the function still
   under-topped-up, leaving the real unclaimed amount at or below zero — `PickQuantityWithGuaranteedStock`
   then returned `0` via `RandomQuantity(1, 0)`'s low/high-swap behavior, and the Released branch's
   `SampleRepository::DecreaseStock` threw `"amount must be > 0"`. This is the same bug behind the
   already-known `DummyDataControllerTest.RunWithDefaultArgumentsSucceedsAndGeneratesNonZeroRecords`
   failure reported by Phase 8's implementation. **Fixed** by adding a `RawUnclaimedStock()` helper
   (unclamped `currentStock - claimed`) and using it inside `TopUpStockIfNeeded` instead of the
   clamped value, so the delta added always closes the true deficit before adding the requested
   surplus.
2. **[Low, fixed] `OrderService`'s three status-error tests only exercised one non-matching status
   each, though the DETAIL doc explicitly asks for all four.** `ApproveOfANonReservedOrderFails...`,
   `RejectOfANonReservedOrderFails...`, and `ReleaseOfANonConfirmedOrderFails...` each only covered
   one of the four "wrong status" cases. **Fix applied:** converted all three to loop over the
   remaining statuses per the spec's own instruction.
3. **[Low, doc-only, fixed] `phase-07-services/STATUS.md` was out of sync with `IMPLEMENT.md`**
   (checkboxes unchecked despite the phase being implemented/committed/green). **Fixed** by updating
   the checklist and adding a verification note.
4. **[Nitpick, acknowledged, no code change] Commit message said the app project "builds cleanly"
   when it only compiles cleanly** (link fails on the pre-existing, out-of-scope missing `main()`,
   same as every phase before main-wiring). Noted here for the record; no fix needed since this is
   a wording-only nitpick about a past commit message, not the code.

## Verdict

Core business logic (settle-then-decide ordering, unclaimed-stock formula, FIFO-chained production
math reuse, upper-case status strings, correct relative includes) matches all three DETAIL docs and
REQUIREMENT.md/ARCHITECTURE.md. The test-coverage gap in finding #1 was not just a documentation
nitpick — closing it caught a real, reproducible correctness bug that a wider random-data run could
have hit unpredictably. All findings addressed; 351/351 tests pass after fixes (up from 291 — Phase
8's UI tests landed in between and are included in this count).
