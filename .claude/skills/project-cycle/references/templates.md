# Document Templates

These are skeletons, not rigid forms — adapt headings to the feature, but keep the substance each section asks for.

## docs/REQUIREMENT.md

```markdown
# Requirement: <feature name>

## Background
Why this is being built — the problem or need driving it.

## Scope
What is in scope for this feature.

## Non-goals
What is explicitly out of scope, so later stages don't quietly expand it.

## Constraints
Technical, platform, or compatibility constraints that shape the design (e.g. must stay header-only, must not add external dependencies, must run on <platform>).

## Acceptance Criteria
A checkable list. Stage 5's final review is graded against this list, so make each item concrete enough to verify:
- [ ] ...
- [ ] ...
```

## docs/ARCHITECTURE.md

```markdown
# Architecture: <feature name>

Satisfies: [docs/REQUIREMENT.md](REQUIREMENT.md)

## Overview
One paragraph: the shape of the solution.

## Components
Each module/class/file involved, its responsibility, and how it fits with existing code (name real files in this repo where relevant, e.g. `JsonParser.h`, `JsonValue.cpp`).

## Data Flow / Interactions
How the components talk to each other for the key scenarios in the requirement doc.

## Key Design Decisions
Decisions worth recording because they weren't obvious — alternatives considered and why this one won.

## Open Questions
Anything left for the implementation-plan stage to resolve (fine to leave empty).
```

## docs/impl/\<feature-slug\>/IMPLEMENT.md

```markdown
# Implementation Plan: <feature name>

Implements: [docs/ARCHITECTURE.md](../../ARCHITECTURE.md)

## Phases

- [ ] Phase 1: <short name> (deps: none)
- [ ] Phase 2: <short name> (deps: Phase 1)
- [ ] Phase 3: <short name> (deps: none)   <!-- e.g. parallelizable with Phase 1 -->

## Phase 1: <short name>

**Depends on:** none
**Touches:** <files/modules>

What this phase implements, in enough detail that someone doing TDD on it doesn't need to re-derive design decisions from ARCHITECTURE.md:
- Behavior to cover with unit tests
- Edge cases that must be tested
- Any interface/signature this phase must expose for later phases

## Phase 2: <short name>

...(same shape)...
```

Each phase gets its own `STATUS.md` alongside this file (e.g. `docs/impl/<feature-slug>/phase-1-<slug>/STATUS.md` if phases have their own subdirectories, or a shared one keyed by phase name — pick whichever matches how you're organizing the phase's code/tests).

## Phase STATUS.md

```markdown
# Status: Phase <N> — <short name>

- [ ] 4-1 Red: tests written
- [ ] 4-2 Green: implementation passes tests
- [ ] 4-3 Refactor: cleaned up, tests still pass
- [ ] 4-4 Verify: behavior matches REQUIREMENT/ARCHITECTURE/plan
- [ ] 4-5 Review: user approved
- [ ] 4-6 Commit: committed

## Notes
Anything worth remembering if this phase is picked back up in a new session — decisions made mid-implementation, deviations from the plan, etc.
```
