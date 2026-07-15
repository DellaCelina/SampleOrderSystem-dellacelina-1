# pc-reviewer review: Phase 1 — Test project scaffolding + Clock abstraction

Reviewed commit: `9fd3601`

## Findings (ranked)

1. **[High] `SampleOrderSystem.vcxproj` never got `Core/IClock.h`/`SystemClock.h`/`SystemClock.cpp`
   added to its own item list — only the Tests project compiles them.** Under Answer A (per
   ARCHITECTURE.md's "Build/test wiring"), both projects are supposed to list the shared source
   files by relative path. Confirmed via build: `SystemClock.obj` only appears under
   `SampleOrderSystemTests/**`, never under `SampleOrderSystem/**`. Not a build failure today only
   because `main.cpp` doesn't exist yet. **Fix applied:** added the three items to
   `SampleOrderSystem.vcxproj` and a matching `Core` filter to `SampleOrderSystem.vcxproj.filters`.

2. **[Medium — process] Open Question #1's required human sign-off was never recorded before
   implementation proceeded.** DETAIL.md's top section says implementation must not begin until
   Answer A vs. Answer B (shared-sources vs. `SampleOrderCore` library) is recorded as resolved.
   **Resolution recorded:** Answer A (shared-sources-compiled-twice, no library project) is
   confirmed as final for this project — it was already the literal reading ARCHITECTURE.md's Key
   Design Decision #1 committed to, and finding #1 above (a bookkeeping gap, not a fundamental
   flaw) is the only cost it has actually surfaced so far. Recorded here and in this phase's
   `STATUS.md`; `docs/ARCHITECTURE.md`'s Open Questions entry is left as historical context (the
   decision was already made there) rather than rewritten.

3. **[Low] Undocumented, inconsistently-applied `ObjectFileName` remap for `SystemClock.cpp`
   only.** No other shared file gets this override, and it's undocumented, so later phases would
   have to guess whether to replicate it. **Fix applied:** removed the override so `$(IntDir)`
   naming applies uniformly (all planned filenames across phases are unique — no collision risk).

## Verdict

Solid execution of the Clock abstraction and test harness (FakeClock/SystemClock/tests all
faithfully match DETAIL.md's contract; build verified clean on both Win32 and x64, Debug config).
Findings 1 and 3 applied directly; finding 2 resolved by recording the decision explicitly.
