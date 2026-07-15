# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Console C++ application: a semiconductor sample production/order management system ("S-Semi").
Customers request samples, an order handler registers orders, a production handler approves/rejects
them and runs a production line for stock shortfalls. Full functional spec lives in
`docs/PREREQUIREMENT.md.txt` and `[CRA_AI] Day3_개인과제_반도체시료관리.pdf` — read both before
designing or implementing anything; they are the source of truth for behavior, not this file.

No application source exists yet — the repo currently contains only an empty Visual Studio C++
console project skeleton (`SampleOrderSystem/SampleOrderSystem.vcxproj`).

## Build

This is a Visual Studio C++ project (`SampleOrderSystem.slnx`), not CMake — no Makefile/CMakeLists.
- Language standard: C++20 (`stdcpp20`), Unicode character set, console subsystem.
- Configurations: Debug/Release x Win32/x64. Platform toolset `v145`.
- Build from Visual Studio, or via MSBuild: `msbuild SampleOrderSystem.slnx /p:Configuration=Debug /p:Platform=x64`.
- No test project or test framework is wired up yet; add one when implementation starts (see
  Implementation guide below re: dummy data generator for test data).

## Domain model (from the spec)

**Order states**: `RESERVED` → (approve) → `CONFIRMED` (stock sufficient) or `PRODUCING` (stock
short, queued to the production line) → `RELEASED` (shipped). Or `RESERVED` → (reject) →
`REJECTED` (terminal, excluded from monitoring). `PRODUCING` → `CONFIRMED` automatically when
production for that order finishes.

**Order numbering**: `ORD-####` (4-digit sequence). Samples have both a sample ID and a name.

**Stock vs. availability are distinct**: current stock includes quantity already claimed by
`PRODUCING`/`CONFIRMED` orders. When approving a new order, only the *unclaimed* remainder counts
as "sufficient stock" — already-reserved-by-other-orders stock is not double-allocated.

**Production queue semantics** (this is the part most likely to be gotten wrong — re-read
`docs/PREREQUIREMENT.md.txt` lines 7–20 before touching this logic):
- The queue is FIFO and persisted to a file, storing each production entry plus its expected
  completion time.
- Queue state is only reconciled lazily, at query time — whenever *any* order status, production
  line status, or stock is queried, first sweep the file for entries whose completion time has
  passed, remove them, and update the corresponding order status (`PRODUCING`→`CONFIRMED`) and
  stock levels accordingly. There is no background timer/thread.
- Only the shortfall computed at approval time enters the queue — not the full order quantity, and
  not quantities from other still-pending/queued orders. Orders for the same sample placed after
  an earlier one may reach `CONFIRMED` before it if stock produced in between happens to cover them.
- Actual production yield is not simulated: real quantity produced = `ceil(shortfall / yield)`,
  and in this system 100% of that produced quantity survives (so stock can end up with surplus
  beyond what any order needs).
- Total production time for a queue entry = `average production time * actual quantity produced`.

**Sample attributes**: sample ID, name, average production time, yield (0–1, e.g. 0.9 = 90
survive per 100 produced).

## Implementation guide (from the spec — external repos to mirror, not copy verbatim)

The assignment requires porting patterns from three sibling PoC repos the author built separately;
consult these repos' structure/approach when implementing the corresponding piece, don't invent an
unrelated design:
- JSON parsing/file persistence: `DataPersistence-dellacelina-1`
- MVC console UI structure: `ConsoleMVC-dellacelina-1`
- JSON-as-database data management: `DataMonitor-dellacelina-1`
- Dummy/test data generation: `DummyDataGenerator-dellacelina-1`

All persistent state (orders, samples/stock, production queue) is stored as JSON files, read/written
directly — no external database.

## Working process for this repo

This repo has a `project-cycle` skill configured (`.claude/skills/project-cycle/SKILL.md`) that
runs feature work through a gated Requirement → Architecture → Phase Plan → TDD Implementation →
Final Review lifecycle via subagent workflows, with a human review gate between each stage. Use it
for building out this system rather than writing ad hoc code — check current stage from
`docs/` + git state each time, per that skill's own instructions, rather than assuming from memory.
