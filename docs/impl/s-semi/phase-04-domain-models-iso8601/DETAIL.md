# Phase 4: Domain models + ISO-8601 timestamp conversion

**Depends on:** Phase 1 (test-scaffolding-clock), Phase 2 (json-value-parser-writer)
**Touches:** `SampleOrderSystem/Models/Sample.h`, `SampleOrderSystem/Models/Sample.cpp`, `SampleOrderSystem/Models/Order.h`, `SampleOrderSystem/Models/Order.cpp`, `SampleOrderSystem/Models/ProductionQueueEntry.h`, `SampleOrderSystem/Models/ProductionQueueEntry.cpp`, `SampleOrderSystem/Core/Iso8601.h`, `SampleOrderSystem/Core/Iso8601.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement the Sample, Order, and ProductionQueueEntry plain data structs plus their ToJson/FromJson pairs, and the single pair of free functions TimePointToIso8601/ParseIso8601, concretely located in SampleOrderSystem/Core/Iso8601.h/.cpp (alongside IClock in Core, but its own header/source pair, not inlined per-model), used by every model's ToJson/FromJson to convert IClock::TimePoint to/from the ISO 8601 UTC string format, so there is exactly one timestamp-formatting code path for the round-trip and FIFO-chain tests to exercise. Depends on IClock's TimePoint type (phase-1) and JsonValue (phase-2) but not on JsonFileStore/SchemaValidator (phase-3), so it can be built in parallel with phase-3. Add Models/*.h/.cpp and Core/Iso8601.h/.cpp to SampleOrderSystemTests.vcxproj so the new tests compile.

## Detail

## Scope

This phase has two independent-but-coupled deliverables:

1. `SampleOrderSystem/Core/Iso8601.h/.cpp` — the single UTC timestamp formatting/parsing code path.
2. `SampleOrderSystem/Models/{Sample,Order,ProductionQueueEntry}.h/.cpp` — plain data structs with `ToJson`/`FromJson`, where only `ProductionQueueEntry` actually has timestamp fields and therefore is the only model that calls into `Iso8601.h`. (The phase's one-line summary says "used by every model's ToJson/FromJson" — that overstates it: per `docs/ARCHITECTURE.md`'s Domain Models section, only `ProductionQueueEntry.enqueuedAt`/`expectedCompletionAt` are timestamps; `Sample` and `Order` have no time fields. Iso8601 still lives as a standalone `Core` utility, not inlined into `ProductionQueueEntry.cpp`, precisely so any *future* model that grows a timestamp field reuses the same path instead of reimplementing formatting.)

This phase does **not** implement `ComputeCompletionTime`/FIFO-chain math (that's `ProductionService`, a later Services-layer phase) — it only guarantees a `ProductionQueueEntry`'s two `TimePoint` fields survive a `ToJson`→`FromJson` round trip exactly, which the later FIFO-chain tests will rely on when they construct entries with a `FakeClock` and check completion-time arithmetic.

Namespace assumption: this plan uses `SampleOrderSystem::Core` (for `IClock`/`Iso8601`), `SampleOrderSystem::Json` (for `JsonValue`), and `SampleOrderSystem::Models` (for the three structs), matching the folder layout in `docs/ARCHITECTURE.md`. If phase-1's `IClock.h` or phase-2's `JsonValue.h` actually landed under different namespace names, use those instead — the load-bearing contract here is the function/struct **signatures and field names**, not the exact namespace spelling.

## `Core/Iso8601.h` / `Core/Iso8601.cpp`

### Required signatures (depended on by phase-3's `Schema`/`SchemaValidator` docs and later Services phases)

```cpp
// SampleOrderSystem/Core/Iso8601.h
#pragma once
#include <string>
#include "IClock.h"

namespace SampleOrderSystem::Core {

// Formats tp as a UTC ISO-8601 string with whole-second precision and a literal "Z" suffix,
// e.g. "2026-07-15T10:30:00Z" — always exactly 20 characters, always zero-padded
// (single-digit month/day/hour/minute/second get a leading zero), never a UTC offset other
// than "Z", never fractional seconds (production time is computed in whole minutes, per
// CLAUDE.md/REQUIREMENT.md, so nothing in this system needs sub-second precision).
// Any sub-second component in tp is truncated (floored, not rounded) and discarded.
std::string TimePointToIso8601(IClock::TimePoint tp);

// Parses a string that must exactly match the "YYYY-MM-DDTHH:MM:SSZ" grammar produced by
// TimePointToIso8601 (fixed length 20, '-' at positions 4/7, 'T' at 10, ':' at 13/16, 'Z' at 19,
// all other positions ASCII digits). Throws std::invalid_argument (message includes the
// offending input) if the length/separators/digit-ness don't match, or if the numeric value is
// calendar-invalid (month outside 1-12, day outside 1-31, or a day that doesn't exist for that
// month/year, e.g. Feb 30; hour/minute/second are still validated against 0-23/0-59/0-59 even
// though this system never emits an out-of-range value itself, since a hand-edited/corrupted
// JSON file could contain one and this must fail loudly rather than construct a nonsensical
// TimePoint).
IClock::TimePoint ParseIso8601(const std::string& text);

} // namespace SampleOrderSystem::Core
```

### Implementation notes (binding — these are the decisions a TDD implementer should not have to re-derive)

- Use C++20 `<chrono>` calendar types (`std::chrono::sys_days`, `std::chrono::year_month_day`, `std::chrono::hh_mm_ss`), **not** `localtime`/`std::localtime`/`std::mktime` and **not** `gmtime` combined with manual struct-tm math. The conversion must be timezone-independent: `IClock::TimePoint` is presumed to alias `std::chrono::system_clock::time_point` (per `SystemClock.h/.cpp` wrapping `std::chrono::system_clock`, per `docs/ARCHITECTURE.md`), whose epoch is UTC — the calendar breakdown must treat it as UTC directly, never adjusting for the build/test machine's local timezone. This is the single highest-risk correctness bug for this phase (a Windows machine not set to UTC would silently produce wrong dates/hours if `localtime` were used), so a dedicated test (below) exists specifically to catch it.
- Truncate to seconds via `std::chrono::time_point_cast<std::chrono::seconds>` (or `floor<seconds>`) before formatting. Since the domain never produces times before the Unix epoch, truncation and flooring coincide — no special negative-duration handling is needed, but don't rely on `time_point_cast`'s round-toward-zero semantics for values before epoch since that would floor incorrectly (out of scope here, just don't accidentally rely on it).
- Format via fixed-width zero-padded output (e.g. `std::snprintf` into a stack buffer, or `std::format` if the toolchain's C++20 `<format>` support is confirmed available — check what phase-1/phase-2 already used elsewhere in the codebase for consistency rather than introducing a second formatting style). Do not use `std::ostringstream` with `std::setw`/`std::setfill` unless that's already the established house style, to avoid mixing formatting approaches across the codebase.
- Parse by validating the fixed grammar character-by-character first (length, literal separators, digit-ness of all other positions) before touching numeric conversion, so a malformed string is rejected uniformly regardless of what garbage is where — don't rely on `sscanf`'s partial-match leniency alone to catch bad input, since e.g. `sscanf("%d-%d-%dT...")` will happily "succeed" on inputs with wrong separator characters in some libc implementations. After the grammar check, construct a `std::chrono::year_month_day` from the numeric year/month/day and check `.ok()` (rejecting e.g. Feb 30) before converting to `sys_days`, then add the `hh:mm:ss` duration, then convert to `system_clock::time_point`.
- No timezone/offset other than literal `Z` is ever accepted or produced — do not build in support for arbitrary `+HH:MM` offsets since nothing in this system needs it and it only adds untested surface area.

### Unit tests (Catch2, suggested file `SampleOrderSystemTests/Core/Iso8601Tests.cpp`)

1. **Format known value.** `TimePointToIso8601` of a `TimePoint` constructed for exactly `2026-07-15 10:30:00 UTC` produces `"2026-07-15T10:30:00Z"`.
2. **Zero-padding.** A `TimePoint` for `2026-01-02 03:04:05 UTC` (single-digit month/day/hour/minute/second) formats as `"2026-01-02T03:04:05Z"`, not `"2026-1-2T3:4:5Z"`.
3. **Sub-second truncation, not rounding.** A `TimePoint` at `2026-07-15 10:30:00.999 UTC` (constructed by adding milliseconds to a whole-second `TimePoint`) formats as `"2026-07-15T10:30:00Z"` — the fractional part is dropped, it does not round up to `:01Z`.
4. **UTC correctness (the timezone-bug regression test).** Pick a `TimePoint` whose UTC clock time and *any plausible non-UTC local time* would fall on different calendar dates/hours if `localtime` were used by mistake (e.g. `2026-07-15T23:30:00Z`, which would roll to `2026-07-16` in UTC+1 or later) and assert the formatted string is exactly the UTC value, independent of the test-runner machine's timezone setting. This test's entire purpose is to fail if a future refactor swaps in `localtime`-based code.
5. **Round trip.** For several representative `TimePoint`s at whole-second precision, `ParseIso8601(TimePointToIso8601(tp)) == tp`.
6. **Round trip via string.** `TimePointToIso8601(ParseIso8601(s)) == s` for a handful of valid strings, confirming parse and format are exact inverses (not just equal-as-TimePoint but re-emit identical text).
7. **Reject malformed strings — one test case per failure mode, each expecting `std::invalid_argument`:**
   - wrong length (too short / too long),
   - missing `T` separator (e.g. space instead),
   - missing trailing `Z` (e.g. `+00:00` offset instead),
   - non-digit character where a digit is required (e.g. `"202X-07-15T10:30:00Z"`),
   - month `00` or `13`,
   - day `00` or `32`,
   - day that doesn't exist in that month (`2026-02-30T00:00:00Z`),
   - hour `24`, minute `60`, second `60`,
   - empty string.

## `Models/Sample.h` / `.cpp`

```cpp
// SampleOrderSystem/Models/Sample.h
#pragma once
#include <string>
#include "../Json/JsonValue.h"

namespace SampleOrderSystem::Models {

struct Sample {
    std::string sampleId;
    std::string name;
    int averageProductionTimeMinutes;
    double yield;
    int currentStock;

    Json::JsonValue ToJson() const;
    static Sample FromJson(const Json::JsonValue& json);
};

} // namespace SampleOrderSystem::Models
```

JSON object field names (must match `schema/sample.schema.json`, which phase-3 introduces but whose field names are already fixed by `docs/ARCHITECTURE.md`): `"sampleId"` (string), `"name"` (string), `"averageProductionTimeMinutes"` (number, whole minutes), `"yield"` (number), `"currentStock"` (number).

**Scope boundary — validation belongs to phase-3, not here.** `Sample::FromJson` performs only *type-level* extraction: it throws `std::invalid_argument` if a required field is absent from the JSON object, or if a field's `JsonValue` variant tag doesn't match what's expected (e.g. `"yield"` is a `String` instead of a `Number`). It does **not** enforce business-rule constraints such as `0 < yield <= 1` or `averageProductionTimeMinutes > 0` — that range/pattern checking is `Persistence::SchemaValidator`'s job in phase-3, which runs *before* `FromJson` is invoked on data coming from a real file load. Unit tests in this phase construct `JsonValue`s directly (bypassing `SchemaValidator`), so out-of-range numeric values (e.g. `yield = 5.0`) are legal inputs to `Sample::FromJson` for the purposes of this phase's tests and must not throw — only structurally missing/mistyped fields throw.

### Unit tests (suggested `SampleOrderSystemTests/Models/SampleTests.cpp`)

1. Round trip: construct a `Sample` with representative field values, `ToJson()` it, `FromJson()` the result, assert every field is equal to the original (field-by-field `REQUIRE`s — no `operator==` is introduced for these structs since nothing in the architecture calls for value comparison outside tests).
2. `FromJson` throws `std::invalid_argument` when a required field (each of the five, one test case per field) is missing from the JSON object.
3. `FromJson` throws `std::invalid_argument` when a field has the wrong JSON type (e.g. `"currentStock"` is a JSON string).
4. `FromJson` does **not** throw on an out-of-domain-but-well-typed value (e.g. `yield = 2.0`), demonstrating the "type-level only" boundary above.

## `Models/Order.h` / `.cpp`

```cpp
// SampleOrderSystem/Models/Order.h
#pragma once
#include <string>
#include "../Json/JsonValue.h"

namespace SampleOrderSystem::Models {

enum class OrderStatus { Reserved, Confirmed, Producing, Released, Rejected };

// Canonical string forms are the upper-case status names ("RESERVED", "CONFIRMED", "PRODUCING",
// "RELEASED", "REJECTED") — matching the requirement doc's own status vocabulary — used as the
// JSON representation of the "status" field.
std::string OrderStatusToString(OrderStatus status);

// Throws std::invalid_argument (message includes the offending string) for any input that isn't
// exactly one of the five canonical strings above (case-sensitive — "reserved" is rejected, not
// silently normalized, since this system never produces lower-case status strings itself and a
// mismatch signals a corrupted file rather than something to be lenient about).
OrderStatus OrderStatusFromString(const std::string& text);

struct Order {
    std::string orderNumber;      // "ORD-####", not validated for format/uniqueness here (see below)
    std::string sampleId;
    std::string customerName;
    int quantity;
    OrderStatus status;

    Json::JsonValue ToJson() const;
    static Order FromJson(const Json::JsonValue& json);
};

} // namespace SampleOrderSystem::Models
```

JSON object field names: `"orderNumber"` (string), `"sampleId"` (string), `"customerName"` (string), `"quantity"` (number), `"status"` (string, one of the five canonical values above).

**Scope boundary, same as `Sample`:** `Order::FromJson` throws `std::invalid_argument` on a missing required field or wrong-JSON-type field, and additionally propagates `OrderStatusFromString`'s exception if `"status"` is well-typed (a JSON string) but not one of the five recognized values. It does **not** validate the `ORD-####` pattern on `orderNumber`, nor `quantity > 0` — those are `SchemaValidator`'s (phase-3) responsibility, and `OrderRepository`'s (a later Repositories phase) responsibility for uniqueness/sequencing. This phase's `Order` is a dumb data holder plus the one piece of genuinely model-owned logic: the status enum's string mapping, since that mapping is intrinsic to what an `Order` *is*, not a cross-cutting schema constraint.

### Unit tests (suggested `SampleOrderSystemTests/Models/OrderTests.cpp`)

1. `OrderStatusToString`/`OrderStatusFromString` round trip for all five enum values (parametrized or five explicit cases).
2. `OrderStatusFromString` throws `std::invalid_argument` on an unrecognized string (e.g. `"UNKNOWN"`, empty string, and the lower-case variant `"reserved"` specifically, to pin the case-sensitivity decision above).
3. `Order` round trip via `ToJson`/`FromJson` for each of the five status values (so the JSON-level status string round-trips correctly for every enum case, not just one).
4. `FromJson` throws `std::invalid_argument` when a required field (each of the five) is missing.
5. `FromJson` throws `std::invalid_argument` when `"status"` is present but not a recognized value (propagated from `OrderStatusFromString`) — assert the exception surfaces from `Order::FromJson`, not swallowed.
6. `FromJson` throws `std::invalid_argument` when a field has the wrong JSON type (e.g. `"quantity"` is a JSON string).

## `Models/ProductionQueueEntry.h` / `.cpp`

```cpp
// SampleOrderSystem/Models/ProductionQueueEntry.h
#pragma once
#include <string>
#include "../Core/IClock.h"
#include "../Json/JsonValue.h"

namespace SampleOrderSystem::Models {

struct ProductionQueueEntry {
    std::string orderNumber;
    std::string sampleId;
    int shortfallQuantity;
    int actualProducedQuantity;
    Core::IClock::TimePoint enqueuedAt;
    Core::IClock::TimePoint expectedCompletionAt;

    Json::JsonValue ToJson() const;
    static ProductionQueueEntry FromJson(const Json::JsonValue& json);
};

} // namespace SampleOrderSystem::Models
```

JSON object field names: `"orderNumber"` (string), `"sampleId"` (string), `"shortfallQuantity"` (number), `"actualProducedQuantity"` (number), `"enqueuedAt"` (string, ISO-8601 via `Core::TimePointToIso8601`/`ParseIso8601`), `"expectedCompletionAt"` (string, ISO-8601, same path).

`ToJson` calls `Core::TimePointToIso8601` for both timestamp fields; `FromJson` calls `Core::ParseIso8601` for both, letting any `std::invalid_argument` it throws propagate uncaught out of `ProductionQueueEntry::FromJson` — this is the one place in this phase where a malformed record is very plausible in practice (a hand-edited or truncated JSON file), and the fail-fast propagation is intentional so a later `JsonFileStore::Load` (phase-3) can catch it as a whole-table load failure per `docs/ARCHITECTURE.md`'s "whole-table, fail-fast" design decision.

Same type-level-only boundary as the other two models applies to the non-timestamp fields (missing/mistyped field → `std::invalid_argument`; out-of-range-but-well-typed numeric values like a negative `shortfallQuantity` are not rejected here).

### Unit tests (suggested `SampleOrderSystemTests/Models/ProductionQueueEntryTests.cpp`)

1. Round trip: construct with representative `TimePoint`s (whole-second precision, per the Iso8601 truncation contract), `ToJson`/`FromJson`, assert every field equal including both `TimePoint`s compared via `==` (`std::chrono::time_point` supports `operator==` directly).
2. `FromJson` throws (propagated `std::invalid_argument` from `ParseIso8601`) when `"enqueuedAt"` or `"expectedCompletionAt"` is a syntactically-wrong ISO-8601 string (e.g. missing `Z`) — one case each field, confirming the propagation path is wired for both timestamp fields independently.
3. `FromJson` throws `std::invalid_argument` when a required non-timestamp field is missing (each of the four: `orderNumber`, `sampleId`, `shortfallQuantity`, `actualProducedQuantity`).
4. `FromJson` throws `std::invalid_argument` when a non-timestamp field has the wrong JSON type.
5. A two-entry scenario constructing entry A's `expectedCompletionAt` and entry B's `enqueuedAt` from the *same* underlying `TimePoint` value (simulating what `ProductionService`'s FIFO chain will later do — reading one entry's completion time as the seed for the next entry's calculation) and confirming both serialize to *identical* ISO-8601 strings — this is the concrete guarantee the later FIFO-chain phase's tests depend on ("exactly one timestamp-formatting code path").

## Build wiring — `SampleOrderSystemTests.vcxproj`

Per `docs/ARCHITECTURE.md`'s Build/test wiring section (Key Design Decision #1/#7), `SampleOrderSystemTests` compiles `SampleOrderSystem`'s non-UI sources directly via `<ClCompile>`/`<ClInclude>` items pointing at relative paths — it does not link a library. This phase must add, alongside whatever `<ClCompile>`/`<ClInclude>` entries phase-1/phase-2 already added for `Core/IClock.h`, `Core/SystemClock.*`, `Json/JsonValue.*`, etc.:

- `<ClInclude Include="..\SampleOrderSystem\Core\Iso8601.h" />` / `<ClCompile Include="..\SampleOrderSystem\Core\Iso8601.cpp" />`
- `<ClInclude Include="..\SampleOrderSystem\Models\Sample.h" />` / `<ClCompile Include="..\SampleOrderSystem\Models\Sample.cpp" />`
- `<ClInclude Include="..\SampleOrderSystem\Models\Order.h" />` / `<ClCompile Include="..\SampleOrderSystem\Models\Order.cpp" />`
- `<ClInclude Include="..\SampleOrderSystem\Models\ProductionQueueEntry.h" />` / `<ClCompile Include="..\SampleOrderSystem\Models\ProductionQueueEntry.cpp" />`

plus the corresponding entries in `SampleOrderSystem.vcxproj` itself (the same four `.h`/`.cpp` pairs) so the main executable also compiles them — the phase's "touches" list already covers both `.vcxproj` files implicitly via the `SampleOrderSystemTests.vcxproj` entry named, but don't forget `SampleOrderSystem.vcxproj` needs the identical additions in its own item list (not cross-project shared items — each `.vcxproj` lists its own copy of the relative paths, per the architecture's chosen mechanism). New test source files (e.g. `Iso8601Tests.cpp`, `Models/SampleTests.cpp`, `Models/OrderTests.cpp`, `Models/ProductionQueueEntryTests.cpp`) also need `<ClCompile>` entries added to `SampleOrderSystemTests.vcxproj` — these are new files owned entirely by this phase, so they don't conflict with anything phase-3 is doing in parallel.

## Dependency/parallelism notes

- Depends on phase-1 (`Core::IClock`, specifically its `TimePoint` type alias) and phase-2 (`Json::JsonValue`'s object/string/number construction and accessors) — both must be genuinely stable interfaces before this phase starts, since every signature above is written directly against them.
- Does **not** depend on phase-3 (`JsonFileStore`/`SchemaValidator`/`Schema`) — this phase's models are tested by constructing `JsonValue`s directly in unit tests, never by going through a file-backed store. This is a real, not just claimed, independence: nothing in `Iso8601.*` or `Models/*.cpp` includes or calls anything under `Persistence/`. Phase-3 and phase-4 touch disjoint file sets (`Persistence/*` vs. `Core/Iso8601.*` + `Models/*`) apart from both needing to add lines to the two `.vcxproj` files — that shared-file edit is the one place a merge conflict is plausible if both phases land at the same time; keep each phase's `.vcxproj` diff to *only* its own new item entries (don't reformat/reorder existing `<ItemGroup>` contents) to keep that conflict trivial to resolve.
- Later phases that depend on this one: `Repositories/SampleRepository`, `Repositories/OrderRepository`, and the production-queue repository (not yet phased) all consume `Sample::ToJson/FromJson`, `Order::ToJson/FromJson`, `ProductionQueueEntry::ToJson/FromJson` directly; `Services/ProductionService` consumes `ProductionQueueEntry`'s `TimePoint` fields and, transitively through them, this phase's `Iso8601` round-trip guarantee for its FIFO-chain completion-time tests.
