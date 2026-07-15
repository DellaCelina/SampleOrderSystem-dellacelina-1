# pc-reviewer review: Phase 2 — JSON value/parser/writer

Reviewed commit: `a07f3eb`

Verified independently: rebuilt both vcxprojs from clean, ran `SampleOrderSystemTests.exe`
("All tests passed (165 assertions in 46 test cases)"). Traced parser logic by hand against the
RFC 8259 grammar — no functional bugs found. Every signature matches DETAIL.md's contract
verbatim (global namespace, vector-of-pairs `ObjectEntries`, `Has`/`TryGet`/`Get` semantics,
`Line()`/`Column()`). All ~36 enumerated test cases present and correctly assert what the spec
says. vcxproj wiring in both `SampleOrderSystem.vcxproj` and `SampleOrderSystemTests.vcxproj` is
strictly additive.

## Findings (ranked)

1. **[Low] `JsonParseException::Column()` counts UTF-8 bytes, not codepoints** — a malformed file
   with Korean text (this system's real data) before a syntax error reports a column ~2-3x too
   large. Not a spec violation (`Column()`'s only contract is "some column recorded"), but wasn't
   called out alongside the similar surrogate-pair/recursion-depth residual risks. **Fix
   applied:** added to DETAIL.md's Residual risks section.
2. **[Very low] Numeric literals that overflow `double` (e.g. `1e400`) are rejected as malformed
   input** rather than silently saturating — a real, deliberate behavior that wasn't documented
   anywhere. **Fix applied:** added to DETAIL.md's Residual risks section.

## Verdict

Solid, spec-conformant implementation. Both findings were documentation gaps, not code defects;
applied directly to DETAIL.md.
