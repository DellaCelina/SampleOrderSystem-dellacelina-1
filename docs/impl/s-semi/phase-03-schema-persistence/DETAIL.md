# Phase 3: Schema documents + persistence layer (JsonFileStore, SchemaValidator)

**Depends on:** Phase 2 (json-value-parser-writer)
**Touches:** `schema/sample.schema.json`, `schema/order.schema.json`, `schema/production_queue.schema.json`, `SampleOrderSystem/Persistence/Schema.h`, `SampleOrderSystem/Persistence/SchemaValidator.h`, `SampleOrderSystem/Persistence/SchemaValidator.cpp`, `SampleOrderSystem/Persistence/JsonFileStore.h`, `SampleOrderSystem/Persistence/JsonFileStore.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Commit the three schema documents (field name/type/required-optional/constraints per architecture, including the 'format: ISO-8601' timestamp declaration), the in-memory Schema descriptor structs parsed from them, a SchemaValidator that checks a JsonValue record array against a Schema and returns a typed error, and JsonFileStore — the generic load(parse+validate)/save(temp-file+atomic-rename) table abstraction with whole-table fail-fast semantics on any malformed record. CLARIFICATION on the format check: SchemaValidator's ISO-8601 format validation is its own self-contained regex/structural check (e.g. matching the YYYY-MM-DDTHH:MM:SSZ shape) and does NOT call phase-4's TimePointToIso8601/ParseIso8601 conversion functions — those exist to give models one code path for actual timestamp *conversion*, whereas SchemaValidator only needs to reject obviously-malformed strings at the persistence boundary. This keeps phase-3 genuinely independent of phase-4 (no hidden dependency), so it can proceed in parallel with phase-4 as originally planned; if a future need arises for SchemaValidator to defer to the same parser for stricter validation, that is a separate follow-up phase, not silently folded in here. This phase depends only on JsonValue/JsonParser/JsonWriter from phase-2. Also add the new Persistence/*.h/.cpp files to SampleOrderSystemTests.vcxproj so the test project can compile the new unit tests against them.

## Detail

## Scope

This phase delivers the schema documents plus the generic schema-validated JSON persistence layer that every repository (phase-5+) will sit on top of. It does **not** touch `TimePointToIso8601`/`ParseIso8601` (phase-4) or any domain model (`Sample`/`Order`/`ProductionQueueEntry`) — those consume this layer but are built separately. Everything here operates purely on `JsonValue` (from phase-2: `SampleOrderSystem/Json/JsonValue.h/.cpp`, `JsonParser.h/.cpp`, `JsonWriter.h/.cpp`).

**Real phase-2 interface (corrects the assumption this section originally guessed at):** everything is in the **global namespace** (no `SampleOrderSystem::Json`), matching phase-1's real committed code. `JsonValue::AsObject() const -> const JsonValue::ObjectEntries&` where `ObjectEntries = std::vector<std::pair<std::string, JsonValue>>` — **not** a `std::map`/`std::unordered_map` as originally guessed below; this doesn't affect this phase since nothing here iterates a record's fields directly, only `Has`/`Get`. `JsonValue::Get` **throws `std::out_of_range`** on a missing key or non-object value (resolved, not left ambiguous); `TryGet` is also available (non-throwing pointer) for callers that prefer to check first. `JsonParser::Parse`'s `JsonParseException` additionally exposes `.Line()`/`.Column()` beyond `.what()`.

Original (superseded) assumption, kept for reference:
- `enum class JsonType { Null, Bool, Number, String, Array, Object };`
- `JsonValue::Type() const -> JsonType`
- Accessors: `AsBool() const -> bool`, `AsNumber() const -> double`, `AsString() const -> const std::string&`, `AsArray() const -> const std::vector<JsonValue>&`, `AsObject() const -> const std::map<std::string, JsonValue>&` (or similar ordered/unordered map — order doesn't matter here since we index by field name)
- `JsonValue::Has(const std::string& key) const -> bool` and `JsonValue::Get(const std::string& key) const -> const JsonValue&` for object field lookup (throws or returns a Null sentinel if absent — validator must not assume either without checking `Has` first)
- `JsonParser::Parse(const std::string& text) -> JsonValue`, throwing `JsonParseException` (has `.what()`) on malformed input.
- `JsonWriter::Write(const JsonValue&) -> std::string` (pretty-printed).

## 1. Schema documents (`schema/*.schema.json`)

Create three committed JSON files. These are read at process startup by `Schema`'s loader (see below) — they are not C++ headers, they're data. Use a small self-describing JSON shape, one object per field:

```json
{
  "table": "samples",
  "fields": [
    { "name": "sampleId", "type": "string", "required": true },
    { "name": "name", "type": "string", "required": true },
    { "name": "averageProductionTimeMinutes", "type": "integer", "required": true, "min": 1 },
    { "name": "yield", "type": "number", "required": true, "exclusiveMin": 0, "max": 1 },
    { "name": "currentStock", "type": "integer", "required": true, "min": 0 }
  ]
}
```

Three files, matching Models in the architecture doc exactly:

- `schema/sample.schema.json` — `table: "samples"`, fields: `sampleId` (string, required), `name` (string, required), `averageProductionTimeMinutes` (integer, required, min 1 i.e. `> 0`), `yield` (number, required, exclusiveMin 0, max 1, i.e. `0 < yield <= 1`), `currentStock` (integer, required, min 0).
- `schema/order.schema.json` — `table: "orders"`, fields: `orderNumber` (string, required, `pattern: "^ORD-\\d{4}$"`), `sampleId` (string, required), `customerName` (string, required), `quantity` (integer, required, min 1), `status` (string, required, `enum: ["Reserved","Confirmed","Producing","Released","Rejected"]`).
- `schema/production_queue.schema.json` — `table: "production_queue"`, fields: `orderNumber` (string, required, same `ORD-####` pattern), `sampleId` (string, required), `shortfallQuantity` (integer, required, min 1), `actualProducedQuantity` (integer, required, min 1), `enqueuedAt` (string, required, `format: "iso8601"`), `expectedCompletionAt` (string, required, `format: "iso8601"`).

Field constraint vocabulary used across all three (this is the complete set `SchemaValidator` must understand — do not invent additional constraint keys without also handling them in the validator):
`type` (`"string"|"integer"|"number"|"boolean"`), `required` (bool, default true if omitted), `min`/`max` (inclusive numeric bounds, applies to `integer`/`number`), `exclusiveMin`/`exclusiveMax` (exclusive numeric bounds), `pattern` (ECMAScript-flavor regex, string fields only, via `std::regex`), `enum` (array of allowed string values, string fields only), `format` (currently only `"iso8601"` is a recognized value; unrecognized `format` values are a schema-authoring bug — assert/throw at schema-load time, not silently ignore).

## 2. `SampleOrderSystem/Persistence/Schema.h`

In-memory descriptor structs, parsed once from the schema JSON files above (no separate `.cpp` needed if kept header-only with inline functions, but a `.cpp` is fine too — this phase doesn't mandate one, `touches` only lists the `.h`, so keep the parsing function inline in the header or add a `Schema.cpp` if the implementer finds that cleaner; if a `.cpp` is added, note the deviation from `touches` in the PR/commit but it's a low-risk, expected adjustment).

```cpp

enum class FieldType { String, Integer, Number, Boolean };

struct FieldSchema {
    std::string name;
    FieldType type;
    bool required = true;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<double> exclusiveMin;
    std::optional<double> exclusiveMax;
    std::optional<std::string> pattern;          // string fields only
    std::optional<std::vector<std::string>> enumValues; // string fields only
    bool isIso8601Format = false;                 // string fields only
};

struct Schema {
    std::string tableName;
    std::vector<FieldSchema> fields;

    // Parses a schema document (already-parsed JsonValue, e.g. from
    // JsonParser::Parse(ReadFile("schema/sample.schema.json"))) into a Schema.
    // Throws std::runtime_error (or a dedicated SchemaParseException) on:
    //   - missing "table" or "fields" keys
    //   - a field missing "name" or "type"
    //   - an unrecognized "type" or "format" value
    //   - "pattern"/"enum" present on a non-string field
    static Schema FromJson(const JsonValue& schemaDoc);
};

```

Note: `Schema::FromJson` parses the schema-*document* shape (the meta-schema above), not a data record. Loading the three `.schema.json` files into `Schema` objects happens once (e.g. in `main.cpp` or a small static registry) and the resulting `Schema` instances are passed into `SchemaValidator`/`JsonFileStore` calls — this phase should provide a small free helper, e.g. `Schema LoadSchemaFromFile(const std::string& path)`, that reads the file and calls `JsonParser::Parse` + `Schema::FromJson`, since every table-loader will need this exact sequence.

## 3. `SampleOrderSystem/Persistence/SchemaValidator.h` / `.cpp`

```cpp

struct ValidationError {
    std::string message;              // human-readable, e.g. "record[2].yield: expected number in (0, 1], got 1.5"
    std::optional<size_t> recordIndex; // which array element failed, if applicable
    std::optional<std::string> fieldName;
};

// Validates that `data` is a JSON array, and that every element is a JSON
// object satisfying every field in `schema` (type, required-ness, numeric
// bounds, pattern, enum, iso8601-shape). Returns std::nullopt on success,
// or the FIRST validation error encountered (fail-fast — does not collect
// every error in the table, just enough to report one clear reason).
std::optional<ValidationError> Validate(const JsonValue& data, const Schema& schema);
```

Behavior details the implementation must get right:
- `data.Type() != JsonType::Array` → error (`recordIndex = std::nullopt`), message like `"expected top-level JSON array"`.
- Each array element must be `JsonType::Object`; non-object element → error with that `recordIndex`.
- For each `FieldSchema` in `schema.fields`, in declaration order:
  - if `!record.Has(name)`: error if `required == true` (message: `"record[i].<name>: missing required field"`); if `required == false`, skip remaining checks for that field on that record.
  - Type check: `String` requires `JsonType::String`, `Integer`/`Number` require `JsonType::Number` (additionally, `Integer` requires the numeric value have no fractional part — check `std::floor(v) == v`), `Boolean` requires `JsonType::Bool`. Mismatch → error naming expected vs actual.
  - Numeric bounds (`Integer`/`Number` only): `min`/`max` inclusive, `exclusiveMin`/`exclusiveMax` exclusive — apply whichever are present; violate any → error.
  - `pattern` (string only): `std::regex_match(value, std::regex(pattern))` — no match → error. (`ORD-####` pattern is `^ORD-\d{4}$`.)
  - `enum` (string only): value must be one of `enumValues` — else error listing the allowed set.
  - `isIso8601Format` (string only): value must structurally match `YYYY-MM-DDTHH:MM:SSZ` — i.e. exactly 20 characters, regex `^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$` **plus** basic range sanity (month 01–12, day 01–31, hour 00–23, minute/second 00–59). This is a self-contained structural check — it does **not** call `TimePointToIso8601`/`ParseIso8601` from phase-4 (those don't exist yet when this phase is implemented, and even once they do, this validator must stay independent per the phase's stated design: it only rejects obviously-malformed strings at the persistence boundary, it is not the place for calendar-correctness like leap-year/day-of-month-31 exactness). Do not import phase-4 headers here.
- Also check for **unknown/extra fields**: NOT required to error — this phase treats extra fields in a record as permissible (forward-compatible), so `Validate` only checks that schema-declared fields are present/correct; it does not reject records with additional keys. (State this explicitly in a test so the behavior is locked in, not accidental.)
- Any additional-property/first-error found short-circuits — return immediately, don't keep scanning.

## 4. `SampleOrderSystem/Persistence/JsonFileStore.h` / `.cpp`

```cpp

class JsonFileStoreException : public std::runtime_error {
public:
    explicit JsonFileStoreException(const std::string& message) : std::runtime_error(message) {}
};

class JsonFileStore {
public:
    // filePath: path to the table's JSON file (e.g. "data/samples.json").
    // schema: the already-parsed Schema for this table.
    JsonFileStore(std::string filePath, Schema schema);

    // Reads the file, parses it as JSON, and validates the resulting value
    // as a JSON array against `schema` via SchemaValidator::Validate.
    // - If the file does not exist: returns an empty JSON array (JsonValue
    //   with Type() == Array, zero elements) — a brand-new table with no
    //   file yet is not an error, it's an empty table. (This matches
    //   OrderRepository's "no separate counter file, empty file/absent
    //   file means max-suffix 0" derivation described in the architecture.)
    // - If the file exists but fails to parse (JsonParseException) OR fails
    //   schema validation (Validate returns an error): throws
    //   JsonFileStoreException, whose message names the file path and
    //   embeds the underlying parse error or ValidationError.message
    //   (including recordIndex/fieldName when available). The WHOLE file
    //   is rejected in this case -- no partial/best-effort load of the
    //   "good" records happens; callers (repository constructors) must not
    //   catch this and silently continue with an empty in-memory table.
    JsonValue Load() const;

    // Writes `data` (must be a JsonValue array) to the table's file:
    //   1. Validate `data` against `schema` first (defense in depth — a
    //      repository should never be able to persist a record that
    //      wouldn't itself pass Load() on the next start). If validation
    //      fails, throws JsonFileStoreException and writes NOTHING (leaves
    //      the existing file, if any, untouched).
    //   2. Serialize via JsonWriter::Write.
    //   3. Write to a temp file in the same directory as filePath (e.g.
    //      "<filePath>.tmp", or a uniquely-suffixed name to avoid collision
    //      with concurrent test runs writing the same directory).
    //   4. Atomically rename/replace the temp file onto filePath
    //      (std::filesystem::rename; on Windows this is atomic for same-
    //      volume renames, which the temp-file-in-same-directory choice
    //      guarantees). If any I/O step fails, throws
    //      JsonFileStoreException and the original file (if it existed)
    //      is left unmodified -- the failed temp file may be left behind
    //      for diagnosis, or cleaned up; either is acceptable, but the
    //      real target file must never be left half-written.
    void Save(const JsonValue& data) const;

    const Schema& GetSchema() const { return schema_; }
    const std::string& GetFilePath() const { return filePath_; }

private:
    std::string filePath_;
    Schema schema_;
};
```

Notes on the temp-file/atomic-rename mechanism: use `std::filesystem::path` for portability; create the temp file path by appending `.tmp` to `filePath_`; write with a `std::ofstream` opened in truncate mode, flush and close it before renaming (renaming over an open handle can fail on Windows); use `std::filesystem::rename(tempPath, targetPath, ec)` with an `std::error_code` overload so a failure can be turned into a `JsonFileStoreException` with a clear message rather than an uncaught `filesystem_error`.

Directory-not-existing on `Save`: if the parent directory of `filePath_` doesn't exist, create it with `std::filesystem::create_directories` before writing the temp file (so a fresh checkout without a pre-existing `data/` folder doesn't require manual setup) — do this in `Save`, not `Load` (Load's "file doesn't exist → empty array" branch should not create directories as a side effect of a read).

## Unit tests to write (GoogleTest/GoogleMock, in `SampleOrderSystemTests` — see phase-1's DETAIL.md superseding note: the test framework switched from Catch2 to GoogleTest/GoogleMock mid-implementation; write these as `TEST(Suite, Case)`/`EXPECT_*`, and use GoogleMock `MOCK_METHOD` where a collaborator needs mocking rather than a hand-rolled fake)

**Schema parsing (`Schema::FromJson`)**
- Valid schema document for each of the three real files parses without throwing and produces the expected field list/types/constraints (spot-check a few fields per table, e.g. `order.schema.json`'s `orderNumber` has `pattern` set and `status` has the 5-value `enumValues`).
- Missing `table` key → throws.
- Missing `fields` key → throws.
- A field object missing `name` → throws.
- A field object missing `type` → throws.
- Unrecognized `type` value (e.g. `"array"`) → throws.
- Unrecognized `format` value (e.g. `"date"`) → throws.
- `pattern` on a non-string-typed field → throws.
- `enum` on a non-string-typed field → throws.

**SchemaValidator::Validate**
- Well-formed array of records matching a schema → returns `std::nullopt`.
- Top-level value is not an array (e.g. an object) → error, no crash.
- An array element that is not an object (e.g. a string or number in the array) → error naming that index.
- Missing required field → error naming record index + field name.
- Missing optional field (construct a schema with a field `required: false`) → no error.
- Wrong type: string where number expected, number where string expected, `true` where string expected → error naming expected vs actual type.
- Integer field given a non-integral number (e.g. `3.5` for `quantity`) → error.
- Numeric field below `min` / above `max` (inclusive boundary values themselves — exactly `min` and exactly `max` — must PASS, one-below/-above must FAIL).
- Numeric field with `exclusiveMin`/`exclusiveMax` (e.g. `yield`): exactly `0` must FAIL (exclusiveMin), exactly `1` must PASS (inclusive `max`), a value `>1` must FAIL.
- `pattern` field: `orderNumber = "ORD-0001"` passes; `"ORD-1"` (wrong digit count), `"ord-0001"` (case), `"ORD-00001"` (5 digits) all fail.
- `enum` field: `status = "Confirmed"` passes; `status = "Cancelled"` (not in the enum) fails.
- `format: iso8601` field: `"2026-07-15T10:30:00Z"` passes; `"2026-07-15 10:30:00"` (no `T`/`Z`) fails; `"2026-07-15T10:30:00"` (missing trailing `Z`) fails; `"2026-13-01T00:00:00Z"` (month 13, out of range) fails; a non-string value in an iso8601 field fails as a type error before format is even checked.
- Extra/unknown field present in a record beyond what schema declares → still passes (locks in the "extra fields tolerated" behavior).
- Multiple records where record[0] is valid and record[1] is invalid → error's `recordIndex == 1` (fail-fast on first bad record, doesn't get confused by an earlier good one).

**JsonFileStore::Load**
- File does not exist at all → `Load()` returns an empty array `JsonValue` (`Type() == Array`, `AsArray().empty()`), no exception.
- File exists, contains valid JSON matching schema → `Load()` returns the parsed array with the same contents (round-trip check: element count and a couple of field values match what was written).
- File exists but contains syntactically invalid JSON (e.g. truncated/missing brace) → `JsonFileStoreException` thrown, message mentions the file path.
- File exists, is valid JSON, but one record fails schema validation (e.g. negative `quantity`) → `JsonFileStoreException` thrown; verify the **whole load fails** even though other records in the same array are individually valid (this is the core "whole-table fail-fast" contract) — assert no partial-success API/return path exists (i.e. there is no way to call `Load()` and get back only the valid subset).
- File exists, is valid JSON, top-level value is an object instead of an array → `JsonFileStoreException` thrown.

**JsonFileStore::Save**
- Saving a valid array to a path whose file doesn't yet exist → file is created; a subsequent `Load()` on a fresh `JsonFileStore` pointed at the same path returns equivalent data (round-trip).
- Saving a valid array to a path whose file already has different (also valid) contents → old contents fully replaced, not merged/appended.
- Saving an array that fails schema validation → `JsonFileStoreException` thrown, and the target file's prior contents (if any) are unchanged (test: write valid content first, then attempt an invalid `Save`, then `Load()` again and confirm it still matches the original valid content, not empty/corrupted/partial).
- Saving to a path whose parent directory does not yet exist → the directory is created and the file written successfully (covers the `data/` folder not existing on a fresh checkout).
- (If feasible in the test harness) verify no stray `.tmp` file is left behind after a *successful* save — check for absence of `<path>.tmp` post-Save. This is a nice-to-have, not a hard requirement if it proves awkward to assert portably.

## Interface surface this phase must expose for downstream phases

- `Schema` (struct), `Schema::FromJson(const JsonValue&) -> Schema`, and a free function `Schema LoadSchemaFromFile(const std::string& path)` — repositories (phase-5+) will call this once per table at construction.
- `FieldType`, `FieldSchema` — exposed for anyone writing additional schema-driven tooling (e.g. `DummyDataGenerator` in a later phase may want to introspect field constraints to generate valid random data).
- `ValidationError`, `Validate(const JsonValue&, const Schema&) -> std::optional<ValidationError>` — usable standalone (e.g. if a future phase wants to validate a single new record before appending, without going through a full `Load`/`Save` cycle).
- `JsonFileStore` — constructed with `(filePath, schema)`, exposing `Load() -> JsonValue` and `Save(const JsonValue&) -> void`, both usable by every repository (`SampleRepository`, `OrderRepository`, `ProductionQueueRepository` in later phases) as their sole means of touching the filesystem for domain data. `JsonFileStoreException` is the one error type repositories need to catch/propagate (per the architecture's "propagates the error up to `main.cpp`, which reports it to console and exits" rule — repositories should NOT catch this themselves and fall back to an empty table).

## vcxproj wiring

Add the following to `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj` as `<ClCompile>`/`<ClInclude>` items pointing at the `SampleOrderSystem` project's files by relative path (per the architecture's decided build mechanism — no shared library, direct source references), matching whatever pattern phase-2 already established for `Json/*`:
- `SampleOrderSystem/Persistence/Schema.h` (and `Schema.cpp` if the implementer adds one)
- `SampleOrderSystem/Persistence/SchemaValidator.h` / `SchemaValidator.cpp`
- `SampleOrderSystem/Persistence/JsonFileStore.h` / `JsonFileStore.cpp`

Also add new test source files under `SampleOrderSystemTests/` for the above (e.g. `SchemaTests.cpp`, `SchemaValidatorTests.cpp`, `JsonFileStoreTests.cpp`) as `<ClCompile>` items in the same project.

## Open items / assumptions to flag to the implementer

- This phase assumes phase-2's `JsonValue` supports the accessor shape described at the top; if phase-2 lands with materially different signatures (e.g. exceptions instead of type-check-then-access), adapt `SchemaValidator`/`JsonFileStore` accordingly — the test list above is the contract to preserve, not the exact C++ calls.
- Whether `data/` and `schema/` are resolved relative to the executable or the CWD is an open question in the architecture doc (not resolved here) — this phase's tests should use explicit absolute/temp-directory paths (e.g. a per-test-run temp directory) rather than hardcoding `"data/..."`/`"schema/..."`, so the resolution-strategy decision (made in a later phase, likely main.cpp wiring) doesn't require rewriting these tests.
- `Schema::FromJson`'s error type: this plan uses `std::runtime_error`-style throwing; if the project later standardizes on a specific exception hierarchy (e.g. mirroring `JsonParseException`), rename accordingly — not a blocking decision for this phase.
