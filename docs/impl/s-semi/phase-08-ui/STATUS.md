# Status: Phase 8 (consolidated) - Remaining UI (Sample UI, Order UI, Monitoring & Production Line UI, Data Monitor & Dummy Data UI)

Merges the former Phase 10 (Sample UI), Phase 11 (Order UI), Phase 12 (Monitoring & Production
Line UI), and Phase 13 (Data Monitor & Dummy Data UI) into this one phase — see IMPLEMENT.md's
consolidation note. Each sub-feature's full TDD detail lives in this directory's
`DETAIL-sample-ui.md`, `DETAIL-order-ui.md`, `DETAIL-monitoring-production-ui.md`, and
`DETAIL-data-monitor-dummy-ui.md` (moved as-is from the original per-phase directories).

**Sample UI is already implemented** (commit `ecab900`, reviewed in `REVIEW-sample-ui.md`) — it was
built and committed before this consolidation happened, back when it was still its own Phase 10.
Merged into this phase's directory purely for bookkeeping; no remaining work for it. The
Red/Green/Refactor/Commit/Review checklist below covers only the three sub-features that are
actually still pending: Order UI, Monitoring & Production Line UI, Data Monitor & Dummy Data UI.

- [ ] 4-1 Red: tests written (Order UI / Monitoring & Production Line UI / Data Monitor & Dummy Data UI only — Sample UI already done)
- [ ] 4-2 Green: implementation passes tests
- [ ] 4-3 Refactor: cleaned up, tests still pass
- [ ] 4-4 Commit: committed
- [ ] 4-5 pc-reviewer (covers verify): reviewed, findings applied

## Notes

(none yet)
