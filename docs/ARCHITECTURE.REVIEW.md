# pc-reviewer review: docs/ARCHITECTURE.md (Stage 2)

Reviewed commit: `da0eeb8`

## Findings (ranked)

1. **Unresolved single-vcxproj-vs-library decision already baked into downstream sections** —
   Components/Build wiring committed to the harder "compile shared sources twice" path while the
   Open Questions section still asked whether that's the right call. **Fix applied:** decided
   concretely — direct relative-path `<ClCompile>`/`<ClInclude>` inclusion in
   `SampleOrderSystemTests.vcxproj`, no library/shared-items project.
2. **Unclaimed-stock zero-floor clamp missing from the authoritative formula** — Key Design
   Decision #2 stated the formula without the `max(0, ...)` clamp; without it the 50/100/100
   acceptance scenario computes shortfall 150 instead of 100. **Fix applied:** added the clamp to
   the formula itself.
3. **Malformed-record failure granularity unspecified** — didn't say whether one bad record fails
   the whole table load or is skipped. **Fix applied:** specified whole-table fail-fast semantics.
4. **Timestamp JSON representation unspecified** — schema examples never said how
   `enqueuedAt`/`expectedCompletionAt` are represented in JSON. **Fix applied:** specified ISO 8601
   UTC string format with a single conversion-function pair.
5. **DummyDataGenerator's status-distribution unspecified** — unclear if it can seed non-Reserved
   orders/queue entries. **Fix applied:** specified generation across all five statuses with
   consistent invariants.

## Verdict

Largely solid and traceable to REQUIREMENT.md, but deferred one foundational build-structure
decision while already assuming its harder consequence, and left several load-bearing details
implicit. All five findings applied directly.
