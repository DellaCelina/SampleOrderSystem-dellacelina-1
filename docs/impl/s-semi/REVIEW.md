# pc-reviewer review: docs/impl/s-semi/IMPLEMENT.md (Stage 3)

Reviewed commit: `ddd78ca`

## Scope of review

Read docs/REQUIREMENT.md, docs/ARCHITECTURE.md, docs/impl/s-semi/IMPLEMENT.md, and closely read
the DETAIL.md files for phases 5, 6, 7, 8, and 9 (repositories, production-service math,
order-service, monitoring/production-view, dummy-data-generator).

Acceptance-criteria-to-phase mapping, dependency graph consistency, and batching were all found
sound — every REQUIREMENT.md acceptance criterion maps to at least one phase, the dependency graph
matches each phase's own "Depends on" line (including the already-fixed phase-9 -> phase-6 drift),
and no batch pairs phases touching the same non-test application file.

## Findings (ranked)

1. **[High] `ProductionService::ComputeCompletionTime` — signature contradiction between phase-6
   and phase-9.** Phase-6 defines a 3-arg contract
   (`enqueuedAt, std::optional<previousTailCompletion>, durationMinutes`); phase-9's call site
   passed 3 different args in a way that would both fail to compile and invert the FIFO
   `max(...)` semantics if naively "fixed" by reordering. **Fix applied:** rewrote phase-9's
   DETAIL.md call site to match phase-6's real contract exactly, via
   `ComputeProductionDurationMinutes` first, then `ComputeCompletionTime(enqueuedAt,
   previousTailCompletion, duration)`.
2. **[High] `ProductionService::Enqueue` — signature contradiction between phase-6 and phase-7.**
   Phase-6 defines `Enqueue(orderNumber, sampleId, shortfallQuantity, clock)` — it computes
   `actualProducedQuantity` internally and returns the entry by value. Phase-7's DETAIL.md assumed
   a different 5-arg shape with `actualProducedQuantity` passed in and no return value. **Fix
   applied:** phase-6's contract kept as source of truth; phase-7's DETAIL.md rewritten to call
   `Enqueue(orderNumber, order.sampleId, shortfall, clock)` and stop calling
   `ComputeActualQuantity` itself before the call.
3. **[Medium-High] Missing `OrderRepository::FindBySampleId` — assumed by phase-7, never declared
   by phase-5.** Phase-7's `ComputeUnclaimedStock` needs an order query scoped by sample; phase-5's
   `OrderRepository` interface didn't expose it. **Fix applied:** added
   `FindBySampleId(sampleId)` to phase-5's DETAIL.md interface and test list.
4. **[Low] Repeated phase-ownership misattribution in prose** — phase-6 and phase-9 DETAIL.md
   both cited domain models as "phase-1's"/"phase-4's" output inconsistently (they're phase-4's).
   Didn't break the dependency graph (phase-6's declared deps transitively cover phase-4 via
   phase-5) but would mislead a reader about which phase must be stable first. **Fix applied:**
   corrected citations in both files.

## Verdict

Phase decomposition, dependency graph, and batching structurally sound and fully cover
REQUIREMENT.md's acceptance criteria, but two DETAIL.md files disagreed with each other on the
actual signature of shared `ProductionService` functions both phase-7 and phase-9 call unmodified
— these were the more dangerous class of gap than the earlier phase-split critique's
dependency-listing catch, since they'd have surfaced as real compile/logic errors mid-TDD rather
than being caught by inspection. All four findings applied directly.
