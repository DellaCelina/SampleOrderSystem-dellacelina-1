# Phase 2: JSON value/parser/writer

**Depends on:** Phase 1 (test-scaffolding-clock)
**Touches:** `SampleOrderSystem/Json/JsonValue.h`, `SampleOrderSystem/Json/JsonValue.cpp`, `SampleOrderSystem/Json/JsonParser.h`, `SampleOrderSystem/Json/JsonParser.cpp`, `SampleOrderSystem/Json/JsonWriter.h`, `SampleOrderSystem/Json/JsonWriter.cpp`, `SampleOrderSystemTests/SampleOrderSystemTests.vcxproj`

## Summary

Implement the recursive JsonValue variant type (Null/Bool/Number/String/Array/Object) with accessors/builders, a JsonParser that turns text into JsonValue and throws JsonParseException on malformed input, and a JsonWriter that pretty-prints JsonValue back to text. No schema/model/domain knowledge belongs here — this is a standalone JSON library mirroring DataPersistence-dellacelina-1's approach, exercised by round-trip parse/write unit tests including malformed-input error cases. Because SampleOrderSystemTests.vcxproj lists its compiled sources explicitly rather than via wildcard, this phase must add the six new JsonValue/JsonParser/JsonWriter .h/.cpp items to that project file as part of the same change, or the new tests silently fail to compile/run.

## Detail

Full phase detail text has been composed above; returning it as the final structured output content is done via the tool call itself. See the assistant message body for the complete text to place under the phase-2 heading in IMPLEMENT.md.

Key file paths referenced (all under C:\workspace\project\SampleOrderSystem):
- SampleOrderSystem/Json/JsonValue.h, JsonValue.cpp
- SampleOrderSystem/Json/JsonParser.h, JsonParser.cpp
- SampleOrderSystem/Json/JsonWriter.h, JsonWriter.cpp
- SampleOrderSystemTests/SampleOrderSystemTests.vcxproj (needs new ClInclude/ClCompile items for the six files above plus a new JsonTests.cpp)
- SampleOrderSystem/SampleOrderSystem.vcxproj (also needs the six files added, since it's the actual owning project per ARCHITECTURE.md's "single project, subfolders" decision — this was not explicitly listed in the phase's `touches` but is necessary; flagged in the detail text as mandatory alongside the test project wiring)
- docs/ARCHITECTURE.md (source of the JSON layer design section consulted: lines 21-24, key design decision 5 at line 89, build/test wiring section lines 56-59)

The detail text covers: JsonValue's variant-based recursive design (order-preserving object via vector-of-pairs, not map — load-bearing for round-trip determinism), full JsonParser grammar and JsonParseException with line/column tracking, JsonWriter pretty-printing rules (key order preserved, integral-double formatting without trailing .0, escape rules), the exact accessor/factory signatures later phases (schema validation, persistence, models) depend on, a full list of unit tests including malformed-input edge cases, and explicit vcxproj wiring instructions for both SampleOrderSystemTests.vcxproj and SampleOrderSystem.vcxproj.
