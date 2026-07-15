# Status: Phase 7 (consolidated) - Remaining services (OrderService, MonitoringService, ProductionLineViewService, DummyDataGenerator)

Merges the former Phase 7 (OrderService), Phase 8 (MonitoringService + ProductionLineViewService),
and Phase 9 (DummyDataGenerator) into one sequential implementation pass — see IMPLEMENT.md's
consolidation note. Each sub-feature's full TDD detail lives in this directory's
`DETAIL-order-service.md`, `DETAIL-monitoring-production-view-service.md`, and
`DETAIL-dummy-data-generator.md` (moved as-is from the original per-phase directories).

- [x] 4-1 Red: tests written
- [x] 4-2 Green: implementation passes tests
- [x] 4-3 Refactor: cleaned up, tests still pass
- [x] 4-4 Commit: committed
- [x] 4-5 pc-reviewer (covers verify): reviewed, findings applied

## Notes

Verified: 351/351 tests pass (up from 291 — Phase 8's UI tests are also included in this count
since both phases now share the same binary). `pc-reviewer` found `DummyDataGenerator`'s test
coverage was thin relative to its own 20-test DETAIL spec and flagged two load-bearing gaps (FIFO
completion-time chaining, and `runningClaims_` seeding from pre-existing orders at construction).
Adding regression tests for exactly those two cases **surfaced a real, previously-latent bug**:
`TopUpStockIfNeeded` computed its top-up delta off `UnclaimedStock()`'s floor-0 (clamped) value
instead of the true (possibly deeply negative) deficit, so when running claims for a sample already
exceeded its stock by more than the requested top-up amount, the "topped up" stock still left the
real unclaimed amount <= 0 — `PickQuantityWithGuaranteedStock` would then return `0` via
`RandomQuantity(1, 0)`'s low/high swap, and the Released branch's `SampleRepository::DecreaseStock`
would throw `"amount must be > 0"`. This is exactly the bug behind the previously-reported
`DummyDataControllerTest.RunWithDefaultArgumentsSucceedsAndGeneratesNonZeroRecords` failure from
Phase 8 — fixing it here resolved that failure too. Fixed by adding `RawUnclaimedStock()` (the
unclamped deficit) and using it inside `TopUpStockIfNeeded` instead of the clamped value. Also
parametrized `OrderService`'s three status-error tests (`Approve`/`Reject`/`Release`) over all
non-matching statuses per the phase's own DETAIL spec, per a second review finding. See REVIEW.md.
