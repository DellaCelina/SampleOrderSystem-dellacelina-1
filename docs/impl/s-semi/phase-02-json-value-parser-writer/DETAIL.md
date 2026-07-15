# Phase 2: JSON value/parser/writer

**Depends on:** Phase 1 (test-scaffolding-clock)
**Touches:** `SampleOrderSystem/Json/JsonValue.h`, `SampleOrderSystem/Json/JsonValue.cpp`, `SampleOrderSystem/Json/JsonParser.h`, `SampleOrderSystem/Json/JsonParser.cpp`, `SampleOrderSystem/Json/JsonWriter.h`, `SampleOrderSystem/Json/JsonWriter.cpp`, `SampleOrderSystem/SampleOrderSystem.vcxproj`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement the recursive `JsonValue` variant type (Null/Bool/Number/String/Array/Object) with accessors/builders, a `JsonParser` that turns text into `JsonValue` and throws `JsonParseException` on malformed input, and a `JsonWriter` that pretty-prints `JsonValue` back to text. No schema/model/domain knowledge belongs here — this is a standalone JSON library mirroring `DataPersistence-dellacelina-1`'s approach, exercised by round-trip parse/write unit tests including malformed-input error cases. Because both `SampleOrderSystem.vcxproj` and `SampleOrderSystemTests.vcxproj` list their compiled sources explicitly rather than via wildcard, this phase must add the six new `.h`/`.cpp` items to **both** project files (not just the test project) as part of the same change, or the app itself and/or the new tests silently fail to compile.

## Detail

## Naming/namespace correction — read this before writing any code

`docs/impl/s-semi/phase-03-schema-persistence/DETAIL.md` and `docs/impl/s-semi/phase-04-domain-models-iso8601/DETAIL.md` were drafted assuming `JsonValue`/`JsonParser`/`JsonWriter` live under a `SampleOrderSystem::Json` namespace and that `JsonValue::AsObject()` returns something map-like. Neither assumption reflects real committed code, because neither phase existed yet when those drafts were written. What phase-1 actually committed (`SampleOrderSystem/Core/IClock.h`, `SystemClock.h/.cpp`, `SampleOrderSystemTests/FakeClock.h`) uses **no namespaces at all** — `class IClock`, `class SystemClock final : public IClock`, `class FakeClock final : public IClock` are all plain global classes. This phase follows that real, established convention, not the draft assumption: `JsonType`, `JsonValue`, `JsonParseException`, `JsonParser`, `JsonWriter` are declared in the **global namespace**, matching `IClock`/`SystemClock`/`FakeClock`.

This is a real, binding deviation from phase-3/phase-4's drafts, and it must be flagged to whoever picks up those phases next: when phase-3/phase-4 are implemented, their `#include "../Json/JsonValue.h"` and any `SampleOrderSystem::Json::JsonValue` qualification must be corrected to plain `JsonValue` (no namespace prefix), and their assumption of a map-typed `AsObject()` must be corrected to the vector-of-pairs shape fixed below (see "Object representation" section). Record this correction in phase-3/phase-4's own STATUS.md notes when work starts there so it isn't rediscovered as a compile error.

## `JsonValue` — recursive variant type

### Object representation: vector-of-pairs, not `std::map`/`std::unordered_map` — this is load-bearing

Decision: `JsonValue`'s Object variant is `std::vector<std::pair<std::string, JsonValue>>`, insertion-ordered, not any kind of map.

Justification:
- **Round-trip determinism is an explicit acceptance criterion** (`docs/REQUIREMENT.md`'s JSON persistence round-trip criterion, and the architecture's "round-trip persistence tests" for every model). If `Object` were a `std::map<std::string, JsonValue>`, keys would silently re-sort alphabetically on every parse, so writing a `Sample` (`sampleId`, `name`, `averageProductionTimeMinutes`, `yield`, `currentStock`) and reading it back would reorder its fields to `averageProductionTimeMinutes, currentStock, name, sampleId, yield` — not wrong data, but a gratuitous diff every time a file is rewritten, and it would make "does this pretty-printed JSON look like what a human wrote" (relevant for schema documents under `schema/*.schema.json`, which are hand-authored) needlessly unstable.
- `DataPersistence-dellacelina-1` (the repo this phase is explicitly mirroring per `CLAUDE.md`/`docs/ARCHITECTURE.md`) uses ordered key-value pairs for exactly this reason — schema documents and data tables read naturally in author-written field order, not lexical order.
- Field lookup by key is `O(n)` under vector-of-pairs instead of `O(log n)`/`O(1)` under a map, but every object in this system (`Sample`, `Order`, `ProductionQueueEntry`, schema field descriptors) has at most ~6 fields — this is not a performance-sensitive path, and the ordering guarantee is worth far more than the lookup complexity here.
- Alternative considered and rejected: `std::map` plus a separate `std::vector<std::string>` recording insertion order, kept in sync. Rejected as needless complexity — two data structures that must never desync for one that already does the job.

### Header

```cpp
// SampleOrderSystem/Json/JsonValue.h
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class JsonType { Null, Bool, Number, String, Array, Object };

// Thrown by any accessor (AsBool/AsNumber/AsString/AsArray/AsObject) when the
// JsonValue's actual Type() does not match what the caller asked for.
// Deliberately a distinct type from JsonParseException (below): this one
// signals "the caller misused an already-successfully-parsed/constructed
// value," not "the input text was malformed" -- callers that want to
// distinguish "field is present but wrong type" from "field is missing"
// should check Type()/Has() first rather than relying on this exception for
// control flow (later phases, e.g. Sample::FromJson, are expected to do
// exactly that).
class JsonTypeException : public std::logic_error {
public:
    explicit JsonTypeException(const std::string& message) : std::logic_error(message) {}
};

class JsonValue {
public:
    using ObjectEntries = std::vector<std::pair<std::string, JsonValue>>;

    // -- Construction: every JsonValue starts life as one concrete variant. --
    JsonValue();                                  // Null
    JsonValue(std::nullptr_t);                     // Null (explicit convenience)
    JsonValue(bool value);
    JsonValue(double value);
    JsonValue(int value);                          // convenience: forwards to double
    JsonValue(const std::string& value);
    JsonValue(const char* value);

    // -- Static factories: preferred way to build Array/Object values, since --
    // -- there is no "vector<JsonValue> means Array" implicit constructor    --
    // -- (an implicit ctor from vector<JsonValue> would collide ambiguously  --
    // -- with wanting numbers/strings inside braced-init in some call sites; --
    // -- explicit factories avoid that ambiguity entirely).                 --
    static JsonValue MakeNull();
    static JsonValue MakeBool(bool value);
    static JsonValue MakeNumber(double value);
    static JsonValue MakeString(std::string value);
    static JsonValue MakeArray();                              // empty array
    static JsonValue MakeArray(std::vector<JsonValue> elements);
    static JsonValue MakeObject();                             // empty object

    JsonType Type() const;
    bool IsNull() const;
    bool IsBool() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsObject() const;

    // -- Read accessors. Each throws JsonTypeException if Type() doesn't --
    // -- match, naming both the expected and actual type in the message. --
    bool AsBool() const;
    double AsNumber() const;
    const std::string& AsString() const;
    const std::vector<JsonValue>& AsArray() const;
    const ObjectEntries& AsObject() const;

    // -- Mutating accessors, for building up Array/Object values in place --
    // -- after construction via MakeArray()/MakeObject() (used by every   --
    // -- later phase's ToJson()). Same throwing behavior as the const     --
    // -- overloads above if Type() doesn't match.                        --
    std::vector<JsonValue>& AsArray();
    ObjectEntries& AsObject();

    // Appends to an Array value. Throws JsonTypeException if this value is
    // not an Array (including if it's a default-constructed Null -- callers
    // must MakeArray() first, Push never silently converts Null-to-Array).
    void Push(JsonValue value);

    // Upserts a key on an Object value: if `key` already exists, its VALUE is
    // replaced in place at its ORIGINAL position (position never moves on
    // update); if `key` does not exist, a new (key, value) pair is appended
    // at the end. Throws JsonTypeException if this value is not an Object.
    void Set(const std::string& key, JsonValue value);

    // Object-only queries. Has() returns false (never throws) if this value
    // is not an Object at all, or if it is an Object without that key --
    // "not an object" and "object without the key" are deliberately not
    // distinguished by Has(), since every real caller's next step is the
    // same either way ("this field isn't available").
    bool Has(const std::string& key) const;

    // Returns a pointer to the value for `key`, or nullptr if absent (or if
    // this value is not an Object). Never throws -- the non-throwing
    // counterpart to Get(), for callers that want to branch on absence
    // without exception-based control flow (e.g. optional schema fields).
    const JsonValue* TryGet(const std::string& key) const;

    // Returns the value for `key`. Throws std::out_of_range (message
    // includes the key name) if this value is not an Object, or is an
    // Object without that key. Callers that need to distinguish "wrong
    // type" from "missing key" should check Type()/Has() first -- Get()
    // collapses both failure modes into the same exception type since, for
    // most call sites (a required field lookup), both are equally fatal.
    const JsonValue& Get(const std::string& key) const;

    // Structural (deep) equality. Object comparison is ORDER-SENSITIVE --
    // two Objects with the same keys/values in different insertion order
    // compare unequal. This is intentional: it matches the round-trip
    // determinism this type exists to provide (see design decision above),
    // and every equality check this system's tests actually need compares
    // a value against itself after a round trip, where order is preserved
    // by construction, not against an independently-authored reordering.
    bool operator==(const JsonValue& other) const;
    bool operator!=(const JsonValue& other) const { return !(*this == other); }

private:
    // Implementation detail, not part of the contract above: back this with
    // std::variant<std::monostate, bool, double, std::string,
    // std::vector<JsonValue>, ObjectEntries> (std::monostate for Null).
    // Nothing outside JsonValue.cpp should touch the variant directly.
};
```

### Behavioral notes an implementer must not have to re-derive

- **`AsNumber()`/number storage is always `double`**, never a separate integer variant. JSON itself has one numeric type; `Integer`-vs-`Number` distinctions (e.g. `quantity` must be a whole number) are a schema-level concern for phase-3's `SchemaValidator`, not something `JsonValue` enforces. `JsonValue(int value)` is provided purely as a convenience overload so call sites like `obj.Set("quantity", 5)` don't require an explicit `static_cast<double>(5)` — it does not create a separate `Integer` `JsonType`.
- **`Push`/`Set` on the wrong variant throw, they do not silently convert.** A `JsonValue` that started as `Null` (default-constructed) is not implicitly promoted to an empty `Array` or `Object` the first time `Push`/`Set` is called on it — callers must start from `MakeArray()`/`MakeObject()`. This keeps "what type is this value" always explicit and matches the parser's own behavior (the parser only ever produces a fully-formed value from a fully-formed JSON token, never a partially-typed one).
- **No implicit conversions between `JsonValue` and C++ scalar types on the accessor side** (e.g. no `operator bool()`, no `operator double()`) — only the explicit `As*()` methods. This avoids silent, surprising conversions when a `JsonValue` is used somewhere a `bool`/`double` was expected by accident.
- **Copy/move semantics:** `JsonValue` should be freely copyable and movable (default-generated special member functions are fine on top of a `std::variant`-based private representation) since every later phase's `ToJson()` builds and returns `JsonValue`s by value.
- **No `JsonValue::MakeInteger`** — deliberately not provided, to avoid implying a JSON-level integer/number distinction that doesn't exist. Later phases needing "is this number actually a whole number" (e.g. `SchemaValidator`'s `Integer` field type check) do their own `std::floor(v) == v` check against `AsNumber()`'s result — that's already how `docs/impl/s-semi/phase-03-schema-persistence/DETAIL.md`'s `SchemaValidator` section is written, and it's consistent with this design.

## `JsonParser` — text to `JsonValue`

### Header

```cpp
// SampleOrderSystem/Json/JsonParser.h
#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include "JsonValue.h"

// Thrown by JsonParser::Parse on any malformed input. Carries enough
// location info (1-based line/column of the character where parsing failed)
// that a caller reporting a load failure (e.g. JsonFileStore in phase-3)
// can produce a message naming both the file and roughly where in it the
// problem is, without JsonParser itself knowing about files.
class JsonParseException : public std::runtime_error {
public:
    JsonParseException(const std::string& message, size_t line, size_t column);

    size_t Line() const { return m_line; }     // 1-based
    size_t Column() const { return m_column; } // 1-based

private:
    size_t m_line;
    size_t m_column;
};

class JsonParser {
public:
    // Parses `text` as a single JSON document and returns the resulting
    // JsonValue. Throws JsonParseException on any malformed input (see
    // grammar and rejection list below). Leading/trailing ASCII whitespace
    // (space, tab, '\n', '\r') around the single top-level value is
    // tolerated; anything else after the top-level value ends is an error
    // ("trailing garbage").
    static JsonValue Parse(const std::string& text);
};
```

### Grammar (RFC 8259-conformant JSON, no extensions)

This parser implements standard JSON exactly — no trailing commas, no comments, no single-quoted strings, no unquoted keys, no `NaN`/`Infinity` literals, no bare leading `+` on numbers. There is no reason to be lenient here: every producer of JSON this system reads is either this same `JsonWriter` or a hand-authored schema/data file that's expected to be valid JSON, and being strict is what lets malformed-input tests (below) pin down exact behavior.

- **Value** := `object` | `array` | `string` | `number` | `"true"` | `"false"` | `"null"`
- **Object** := `{` (whitespace) `}` | `{` `member` (`,` `member`)* `}` — `member` := `string` `:` `value`. Trailing comma after the last member is a parse error. Unquoted keys are a parse error. Duplicate keys within one object are **not** a parse error (see below).
- **Array** := `[` (whitespace) `]` | `[` `value` (`,` `value`)* `]`. Trailing comma is a parse error.
- **String** := `"` (`char`)* `"`, where `char` is any Unicode scalar value except `"`, `\`, and the ASCII control characters `0x00`-`0x1F` (all of which **must** be represented via an escape, per RFC 8259 — a raw/literal control byte inside a string, e.g. a literal tab character between the quotes, is a parse error, not tolerated leniency), or one of the escapes `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, `\uXXXX` (exactly 4 hex digits, case-insensitive). Surrogate pairs (`\uD800`-`\uDFFF` pairs for astral characters) are accepted structurally (each `\uXXXX` decoded independently and its UTF-8 encoding appended) but this parser does **not** validate that a high surrogate is followed by a matching low surrogate — that's an accepted, documented limitation (see Residual risks) since nothing in this system's actual data needs characters outside the Basic Multilingual Plane.
- **Number** := `-`? `int` `frac`? `exp`?, where `int` is `0` or a non-zero digit followed by more digits (**a leading zero followed by more digits, e.g. `01`, is a parse error** — standard JSON grammar), `frac` is `.` followed by one or more digits (a bare trailing `.` with no digits after it, e.g. `1.`, is a parse error), `exp` is (`e`|`E`) (`+`|`-`)? one or more digits. A lone `-` with no following digit is a parse error.
- **Whitespace** (between tokens only, never inside a string except via escapes) := any run of `' '`, `'\t'`, `'\n'`, `'\r'`.

### Duplicate object keys

If the same key appears more than once in one object literal, this is **not** an error. The resulting `JsonValue::Object`'s value for that key is whichever value was written **last** in the source text (last-value-wins), but its **position** in the resulting `ObjectEntries` vector is the position of its **first** occurrence — this falls directly out of building the object via repeated `Set()` calls in encounter order, since `Set()`'s upsert semantics (value replaced in place, position unchanged on update) are exactly this rule. Add a dedicated test for this (see below) so it's locked in as intentional, not accidental.

### Numeric precision

Numbers are stored as `double`. Integers up to 2^53 round-trip exactly (every quantity in this system — stock counts, order quantities, minute-granularity production times, 4-digit order sequence numbers — is far below that threshold, so this is not a practical limitation for this project). No arbitrary-precision/bignum handling is implemented or needed.

### Implementation approach

A straightforward single-pass recursive-descent parser over the input `std::string`, tracking a cursor index plus running line/column counters (increment line and reset column to 1 on each `'\n'` consumed; column increments on every other character consumed) so `JsonParseException` can report an accurate location. Suggested structure: a private `Cursor` helper (position + line + column + the source string) threaded through `ParseValue`/`ParseObject`/`ParseArray`/`ParseString`/`ParseNumber`/`ParseLiteral` private static/free functions in `JsonParser.cpp`; `Parse` calls `SkipWhitespace`, `ParseValue`, `SkipWhitespace`, then checks the cursor is at end-of-input (else "trailing garbage after JSON value" error at the cursor's current line/column).

No recursion-depth guard is implemented (out of scope — this system's inputs are small, hand-authored or self-produced JSON files, not adversarial input); call this out as a residual risk below rather than silently building in a limit nobody asked for.

## `JsonWriter` — `JsonValue` to pretty-printed text

### Header

```cpp
// SampleOrderSystem/Json/JsonWriter.h
#pragma once
#include <string>
#include "JsonValue.h"

class JsonWriter {
public:
    // Serializes `value` to a pretty-printed JSON string: 2-space indent per
    // nesting level, one member/element per line for non-empty
    // Object/Array, object key order exactly matching JsonValue's own
    // insertion order (never re-sorted), no trailing newline at the very
    // end of the returned string. See JsonWriter.cpp for the exact
    // per-JsonType formatting rules (number formatting, string escaping).
    static std::string Write(const JsonValue& value);
};
```

### Pretty-printing rules (binding — pin these down now so writer output is deterministic and testable)

- **Indentation:** 2 spaces per nesting level. A newline follows every `{`/`[` that opens a non-empty Object/Array, and every member/element line ends with `,` (except the last, which has none) followed by a newline before the closing `}`/`]`, which is indented to match the opening brace's own indentation level.
- **Empty containers:** an empty Object renders as `{}` (no internal newline/indentation); an empty Array renders as `[]`. Never `{\n}` or `[\n]`.
- **Object key order:** exactly the order stored in `JsonValue`'s `ObjectEntries` (insertion order, per the design decision above) — never alphabetically sorted, never reordered in any way.
- **Object member separator:** `"key": value` — a single space after the colon, no space before it.
- **Numbers:** formatted via `std::to_chars` (the `<charconv>` floating-point overload, C++17, available under this project's `v145` toolset/C++20 standard) using its default (shortest round-trip) formatting. This single choice satisfies two requirements at once without special-casing: whole-number values like `5.0` render as `"5"` (no trailing `.0`, satisfying the "no gratuitous `.0` on integral fields like `currentStock`/`quantity`" expectation) because `to_chars`'s shortest-round-trip output for an exact integral double omits the fractional part entirely, while fractional values like `0.9` (a `yield` value) render as `"0.9"`. Do not hand-roll a "check if integral, branch to `%d`-style formatting else `%f`" scheme — `to_chars` already produces the right output for both cases uniformly and is guaranteed to round-trip exactly back through `std::from_chars`/`strtod`-equivalent parsing, which is what `JsonParser::ParseNumber` must use on the read side (use `std::from_chars` in the parser too, or an equivalent exact-parsing routine — not `atof`, whose locale-dependent behavior is exactly the kind of subtle bug this note exists to head off).
- **Strings:** wrapped in `"`, with these characters escaped: `"` → `\"`, `\` → `\\`, `0x08` → `\b`, `0x09` → `\t`, `0x0A` → `\n`, `0x0C` → `\f`, `0x0D` → `\r`; every other ASCII control character (`0x00`-`0x1F` not covered above) → `\u00XX` (lower-case hex, 4 digits, e.g. `0x01` → ``). Bytes `0x20` and above, including raw multi-byte UTF-8 sequences, are passed through unescaped — this is valid per RFC 8259 (JSON strings may contain raw UTF-8; escaping is only *required* for `"`, `\`, and control characters) and matches what `JsonParser` accepts on read, so a string containing e.g. Korean customer names round-trips byte-for-byte without unnecessary `\uXXXX` escaping.
- **Booleans/null:** literal `true`/`false`/`null`, unquoted.
- **No trailing newline** after the final `}`/`]`/scalar token of the top-level value (callers that want one, e.g. when writing a file, add it themselves).

## Interface surface / exact signatures phase-3 and phase-4 depend on

Both already-drafted downstream phases were written against a *guessed* API before this phase existed. The following is the actual, binding contract they must be adapted to (their DETAIL.md files should be corrected to match when those phases start, per the naming-correction note above):

- `enum class JsonType { Null, Bool, Number, String, Array, Object };` — as guessed, this part was right.
- `JsonValue::Type() const -> JsonType` — matches phase-3's assumption.
- `JsonValue::AsBool() const -> bool`, `AsNumber() const -> double`, `AsString() const -> const std::string&`, `AsArray() const -> const std::vector<JsonValue>&` — all match phase-3's assumption exactly.
- `JsonValue::AsObject() const -> const JsonValue::ObjectEntries&` where `ObjectEntries = std::vector<std::pair<std::string, JsonValue>>` — **differs from phase-3's `std::map`/`std::unordered_map` guess.** Phase-3's `SchemaValidator` (which only needs to iterate `schema.fields` and look up each by name via `record.Has(name)`/`record.Get(name)`) is unaffected by this difference since it never iterates a record's fields directly — it only calls `Has`/`Get`, both of which exist with the exact signatures phase-3 assumed.
- `JsonValue::Has(const std::string&) const -> bool` and `JsonValue::Get(const std::string&) const -> const JsonValue&` (throws `std::out_of_range` if absent/not-object) — matches phase-3's assumption ("throws or returns a Null sentinel if absent — validator must not assume either without checking Has first"); this phase resolves that open question in favor of **throws**, plus additionally exposes `TryGet` (non-throwing pointer) for callers that prefer to avoid exception-based control flow.
- No namespace (global) — **differs from both phase-3 and phase-4's `SampleOrderSystem::Json::JsonValue` assumption.** Use bare `JsonValue`, `JsonType`, etc.
- `JsonParser::Parse(const std::string&) -> JsonValue`, throwing `JsonParseException` (has `.what()` via its `std::runtime_error` base, plus `.Line()`/`.Column()`) on malformed input — matches phase-3's assumption, with the addition of `Line()`/`Column()` beyond the bare `.what()` phase-3 guessed at.
- `JsonWriter::Write(const JsonValue&) -> std::string` — matches phase-3's assumption exactly.
- Construction/building surface consumed by phase-4's `Sample`/`Order`/`ProductionQueueEntry::ToJson()` implementations: `JsonValue::MakeObject()` then repeated `.Set(key, value)` calls (each `value` built via the `JsonValue(bool)`/`JsonValue(double)`/`JsonValue(int)`/`JsonValue(const std::string&)` implicit constructors, or nested `MakeObject()`/`MakeArray()` calls for nested structures — none of the domain models in phase-4 actually nest objects/arrays, but `JsonFileStore` in phase-3 will, wrapping each table's records in a top-level `MakeArray()`).

## Unit tests to write (Catch2, suggested files under `SampleOrderSystemTests/Json/`)

### `JsonValueTests.cpp`

1. Default-constructed `JsonValue` is `Null`; `Type() == JsonType::Null`, `IsNull()` true, all other `Is*()` false.
2. `JsonValue(true)`/`JsonValue(false)` — `Type() == Bool`, `AsBool()` returns the constructed value.
3. `JsonValue(3.5)` and `JsonValue(5)` (int overload) — `Type() == Number`, `AsNumber()` returns `3.5` and `5.0` respectively exactly.
4. `JsonValue(std::string("hello"))` and `JsonValue("hello")` (const char*) — `Type() == String`, `AsString()` returns `"hello"`.
5. `MakeArray()` then `Push` three heterogeneous values (a number, a string, a nested object) — `Type() == Array`, `AsArray().size() == 3`, each element has the expected type/value.
6. `MakeArray()` with zero `Push` calls — `AsArray().empty()` true, `Type() == Array` (not `Null`).
7. `MakeObject()` then `Set("a", 1)`, `Set("b", "two")`, `Set("c", true)` — `AsObject()` has 3 entries **in that exact insertion order** (assert on `AsObject()[0].first == "a"`, etc., not just `Has`/`Get`).
8. `Set` called twice with the same key (`Set("a", 1)` then later `Set("a", 2)`, with an intervening `Set("b", ...)` in between) — final `AsObject()` has 2 entries, `"a"` is still at index 0 (position unchanged) with value `2` (last-write-wins on value).
9. `Has`/`Get`/`TryGet` on a present key — `Has` true, `Get` returns the right value, `TryGet` returns a non-null pointer to the same value.
10. `Has`/`TryGet` on an absent key — `Has` false, `TryGet` returns `nullptr`; `Get` throws `std::out_of_range`.
11. `Has`/`TryGet`/`Get` called on a non-Object value (e.g. a `Number`) — `Has` false, `TryGet` returns `nullptr`, `Get` throws `std::out_of_range` (not `JsonTypeException` — Get's contract collapses both failure modes per the header comment above).
12. Calling `AsBool()` on a `Number`, `AsNumber()` on a `String`, `AsString()` on a `Bool`, `AsArray()` on an `Object`, `AsObject()` on an `Array` (one case each, covering every accessor/wrong-type combination at least once) — each throws `JsonTypeException`.
13. `Push` called on a non-Array value (e.g. freshly default-constructed `Null`, or a `Number`) — throws `JsonTypeException`, does not silently convert to an Array.
14. `Set` called on a non-Object value — throws `JsonTypeException`, does not silently convert.
15. `operator==`: two independently built equal nested structures (object containing an array containing an object) compare equal; changing any single leaf value, or the order of an object's keys, makes them compare unequal (locks in order-sensitivity).
16. Copy of a `JsonValue` (Array/Object case in particular) is independent of the original — mutating the copy's `AsArray()`/`AsObject()` does not affect the original (value semantics, no accidental shared mutable state).

### `JsonParserTests.cpp`

1. Parse `"true"`, `"false"`, `"null"` — correct `Type()`/`AsBool()` for the first two, `IsNull()` for the third.
2. Parse a plain integer (`"42"`), a negative integer (`"-7"`), zero (`"0"`), a decimal (`"3.14"`), a negative decimal (`"-0.5"`), exponent forms (`"1e10"`, `"1.5E-3"`, `"2e+5"`) — `AsNumber()` matches expected `double` value in each case.
3. Parse a string with every named escape: `"\"a\\b/c\bd\fe\nf\rg\th\""` — `AsString()` equals the corresponding literal characters.
4. Parse a string with a `\uXXXX` escape for a BMP character (e.g. `é` → "é") — `AsString()` contains the correctly UTF-8-encoded character.
5. Parse an empty array `"[]"` and empty object `"{}"` — correct `Type()`, empty contents.
6. Parse a nested structure: an array of objects, each with string/number/bool/null/nested-array fields — walk the result with `AsArray()`/`AsObject()`/`Get` and assert every leaf value matches what was written in the source text (this is the "full nested round trip" smoke test for the grammar as a whole).
7. Leading/trailing whitespace (spaces, tabs, newlines) around a valid top-level value is tolerated — `Parse("  \n\t{ \"a\" : 1 }\n  ")` succeeds with the same result as `Parse("{\"a\":1}")`.
8. **Duplicate keys in one object**: `Parse(R"({"a":1,"b":2,"a":3})")` — result has exactly 2 entries, `"a"` at index 0 with value `3`, `"b"` at index 1 with value `2` (see "Duplicate object keys" section above — pin this down explicitly, don't leave it as accidental behavior).
9. Malformed input — one `TEST_CASE`/`SECTION` per case below, each asserting `JsonParser::Parse(...)` throws `JsonParseException`:
   - empty string (`""`)
   - `"{"` (unterminated object)
   - `"["` (unterminated array)
   - `"123 abc"` (trailing garbage after a complete value)
   - `R"({"a" 1})"` (missing colon)
   - `"[1 2]"` (missing comma between array elements)
   - `R"({"a":1 "b":2})"` (missing comma between object members)
   - `"{a:1}"` (unquoted key)
   - `"{'a':1}"` (single-quoted key/string)
   - `"[1,2,]"` (trailing comma in array)
   - `R"({"a":1,})"` (trailing comma in object)
   - `R"("\q")"` (invalid escape sequence)
   - `R"("\u12")"` (incomplete unicode escape, fewer than 4 hex digits)
   - `R"("\uZZZZ")"` (non-hex digits in a unicode escape)
   - `"01"` (leading zero followed by more digits)
   - `"1."` (decimal point with no digits after it)
   - `"-"` (bare minus, no digit)
   - `"NaN"` and `"Infinity"` (rejected — not valid JSON literals)
   - a string containing a raw/literal unescaped tab character between the quotes (control character not escaped)
10. `JsonParseException` carries a location: construct a multi-line malformed input where the error is on line 2 (e.g. `"{\n  \"a\": ,\n}"`, a missing value after the colon) and assert `Line() == 2` (exact column is implementation-defined enough that the test should assert `Line()` precisely but only assert `Column() > 0`, i.e. "some column was recorded," to avoid over-pinning an off-by-one that doesn't actually matter to any caller).

### `JsonWriterTests.cpp`

1. `Write(JsonValue())` (Null) → `"null"`. `Write(JsonValue(true))` → `"true"`. `Write(JsonValue(false))` → `"false"`.
2. `Write(JsonValue(5))` → `"5"` (no trailing `.0`). `Write(JsonValue(0.9))` → `"0.9"`. `Write(JsonValue(-3))` → `"-3"`.
3. `Write(JsonValue(std::string("hi")))` → `"\"hi\""`.
4. String escaping: a string containing `"`, `\`, and `\n` writes with each escaped correctly (assert the exact expected output string, e.g. input `a"b\c` + newline → `"a\"b\\c\n"` inside the JSON quotes).
5. A control character not covered by a named escape (e.g. `0x01`) writes as ``.
6. `Write(JsonValue::MakeArray())` → `"[]"`. `Write(JsonValue::MakeObject())` → `"{}"`.
7. A small nested fixture (an object with 2 scalar fields and one nested array of 2 numbers) writes to an **exact, fully pinned expected string** with 2-space indentation — this test should assert the literal expected multi-line string byte-for-byte, to lock in the indentation/newline/comma-placement rules described above (don't just assert "parses back equal," since that alone wouldn't catch a merely-ugly-but-still-valid formatting regression).
8. Object key order in output exactly matches insertion order for a `Set` sequence that is deliberately non-alphabetical (e.g. `Set("zebra", 1)` then `Set("apple", 2)`) — assert `"zebra"` appears before `"apple"` in the output text.
9. Round trip via structural equality: for several representative `JsonValue` trees built with the builders (not parsed from text), `JsonParser::Parse(JsonWriter::Write(v)) == v` using `operator==`.
10. Round trip via text: for several representative valid JSON input strings (including nested structures), `JsonWriter::Write(JsonParser::Parse(text))` re-parsed again equals (`operator==`) the original `JsonParser::Parse(text)` result — i.e. writing and re-parsing is idempotent from the second parse onward, even though the exact whitespace of the written text legitimately differs from the original input text.

## vcxproj wiring (both projects — this is the part most likely to be silently skipped)

### `SampleOrderSystem/SampleOrderSystem.vcxproj`

Add to the existing `<ItemGroup>` that currently contains the `Core/*` entries (see the file's current contents — do not create a second `<ItemGroup>`, append to the existing one to match phase-1's pattern):

```xml
<ClInclude Include="Json\JsonValue.h" />
<ClInclude Include="Json\JsonParser.h" />
<ClInclude Include="Json\JsonWriter.h" />
```
and
```xml
<ClCompile Include="Json\JsonValue.cpp" />
<ClCompile Include="Json\JsonParser.cpp" />
<ClCompile Include="Json\JsonWriter.cpp" />
```

This is the file most likely to be forgotten, because none of `SampleOrderSystem.vcxproj`'s existing tests exercise it directly (only `SampleOrderSystemTests.vcxproj` does) — but per `docs/ARCHITECTURE.md`'s Build/test wiring section, `SampleOrderSystem` is the actual owning/shipping project for this source under the "single project, subfolders" decision, and every later phase (`Persistence/JsonFileStore`, `Models/*`, eventually `main.cpp`) needs these files compiled into the real app binary, not only the test binary. Skipping this addition would let the test suite pass while the actual `SampleOrderSystem.exe` fails to build once a later phase's app code tries to `#include "Json/JsonValue.h"`.

### `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

Add to the existing `<ItemGroup>`s (the ones currently holding `catch_amalgamated.hpp`/`FakeClock.h`/the `..\SampleOrderSystem\Core\*` entries — append, don't create new groups):

```xml
<ClInclude Include="..\SampleOrderSystem\Json\JsonValue.h" />
<ClInclude Include="..\SampleOrderSystem\Json\JsonParser.h" />
<ClInclude Include="..\SampleOrderSystem\Json\JsonWriter.h" />
```
and, in the `<ClCompile>` group:
```xml
<ClCompile Include="..\SampleOrderSystem\Json\JsonValue.cpp" />
<ClCompile Include="..\SampleOrderSystem\Json\JsonParser.cpp" />
<ClCompile Include="..\SampleOrderSystem\Json\JsonWriter.cpp" />
<ClCompile Include="Json\JsonValueTests.cpp" />
<ClCompile Include="Json\JsonParserTests.cpp" />
<ClCompile Include="Json\JsonWriterTests.cpp" />
```

The `AdditionalIncludeDirectories` entry (`..\SampleOrderSystem;%(AdditionalIncludeDirectories)`) already added in phase-1 is sufficient for `#include "Json/JsonValue.h"` to resolve from the test project's own new `Json/*Tests.cpp` files — no additional include-path changes are needed.

### Parallelism note for whoever schedules phase batches

`docs/impl/s-semi/IMPLEMENT.md`'s "Batch B" runs Phase 2 and Phase 4 in parallel (both depend only on Phase 1, and their own source files — `Json/*` vs `Models/*` + `Core/Iso8601.*` — are disjoint). Both phases, however, add lines to the same two `.vcxproj` files. This is a real, known overlap, not a hidden one (`IMPLEMENT.md` already flags "shared vcxproj item-list edits" as expected and sets `overlappingFiles: true` for exactly this reason) — when implementing this phase concurrently with phase-4, keep this phase's `.vcxproj` diff strictly additive (new `<ClInclude>`/`<ClCompile>` lines appended to the existing groups) and avoid reordering or reformatting any existing `<ItemGroup>` content, so the two phases' edits merge as a trivial union rather than a real conflict.

## Residual risks / things not resolved by this phase

- No recursion-depth guard in the parser against pathologically deeply nested input — acceptable because every JSON this system parses is either self-produced by `JsonWriter` or a small, hand-authored schema/data file, never adversarial/external input.
- `\uD800`-`\uDFFF` surrogate-pair handling in string escapes is structurally accepted but not validated for well-formed pairing — acceptable because nothing in this system's actual field values (sample names, customer names, IDs) needs characters outside the Basic Multilingual Plane; flagged here rather than silently assumed correct.
- Exact `JsonParseException::Column()` values are implementation-defined precision (tests only assert `Line()` exactly and `Column() > 0`) — if a later phase's error-reporting UI wants a precisely-pinned column convention (e.g. "points at the first offending character" vs "points at the token start"), that should be decided and tested explicitly at that point, not assumed from this phase's loose test.
