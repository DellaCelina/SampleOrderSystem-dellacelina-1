---
name: project-cycle
description: Run this project's gated development lifecycle as a dynamic multi-agent workflow — Requirement → Architecture → Implementation Plan → TDD Implementation (per phase, parallel where possible) → Final Review. Each stage's document is drafted and critiqued by subagents via the Workflow tool. The stage gate is a `pc-reviewer` subagent, not the human: commit the stage's output first, then have `pc-reviewer` review it, apply its findings, commit again, and auto-advance to the next stage — continue this all the way through Stage 5 without stopping for the user unless genuinely blocked. Use whenever the user asks to start/continue a new feature in this repo, says things like "다음 단계로 진행해줘", "요구사항 작성해줘", "phase 구현 시작해줘", or asks "지금 어느 단계야?". Always check current state from the repo (docs/ + git) before acting — never assume the stage from conversation memory alone.
---

# Project Cycle

A fixed 5-stage lifecycle for building a feature in this repo. The user has explicitly asked for this to run as a **dynamic workflow with subagents**, not a single agent writing documents alone — so each stage's actual drafting/implementation work is delegated to a `Workflow` script under `.claude/workflows/` that fans out to subagents (a drafter + an independent critic, parallel phase writers, parallel TDD chains, parallel review lenses). That user request is the standing authorization to invoke these workflows for this skill; you don't need to re-ask each time.

**The review gate is now a subagent, not the human.** The user has explicitly asked to run the full cycle autonomously: when a stage produces something reviewable, commit it yourself first (the commit is the checkpoint, not a request for permission), then invoke the `pc-reviewer` agent (via the `Agent` tool, `subagent_type: "pc-reviewer"`) against what's now on disk. `pc-reviewer` always returns at least one concrete finding — apply it (small fixes: patch directly; larger ones: loop back into the stage's own drafting step with the finding folded in), commit the fix, and move on to the next stage without stopping. Keep doing this through every stage until Stage 5's final review is committed. Only stop mid-cycle if you hit a genuine blocker (a decision only the user can make, an ambiguity `pc-reviewer` and you can't resolve from the docs/code, or a repeated failure) — not merely because a stage finished.

## The agent roster

Every `agent()` call in these workflows uses a purpose-built subagent type from `.claude/agents/`, not generic `general-purpose` — each has a scoped toolset and a persona tuned to one job, so quality doesn't depend on re-explaining the role in every prompt, and read-only roles (analyst, architect, planner, critic, reviewer, verifier) can't quietly patch around a problem instead of reporting it:

| Agent | Role | Tools | Used in |
|---|---|---|---|
| `pc-analyst` | 기획자 — turns a request into a checkable requirement | read-only | Stage 1 draft/revise |
| `pc-architect` | 설계자 — designs against the real codebase | read-only | Stage 2 draft/revise |
| `pc-planner` | 계획자 — splits architecture into dependency-aware phases | read-only | Stage 3 split/revise/expand |
| `pc-critic` | 비평가 — adversarial pre-review of a draft doc, defaults to skeptical | read-only | Stage 1/2/3 critique steps (inside the drafting workflow, before commit) |
| `pc-reviewer` | 검토자 — post-commit stage gate, always returns ≥1 actionable finding; for Stage 4 also covers verify (no separate per-phase verify step anymore) | read-only + Bash | After every stage's commit (1, 2, 3, each Stage 4 phase, Stage 5) |
| `pc-tester` | 테스터 — writes a few focused Red-state tests (core path + highest-value edge cases, not exhaustive); reviews test coverage in final review | full | Stage 4 (4-1), Stage 5 test-coverage lens |
| `pc-implementer` | 구현자 — minimal Green implementation, stays in phase scope | full | Stage 4 (4-2) |
| `pc-refactorer` | 리팩터러 — cleans up without changing behavior; reviews code quality in final review | full | Stage 4 (4-3), Stage 5 code-quality lens |
| `pc-verifier` | 검증자 — checks behavior against spec, can't edit so its verdict stays honest | read-only + Bash | Stage 5 requirement-coverage lens only (per-phase verify is now folded into `pc-reviewer`) |

Reuse is deliberate, not laziness: `pc-critic` reviewing three different doc types, `pc-reviewer` gating every stage the same way, or `pc-tester`/`pc-refactorer`/`pc-verifier` doing double duty in final review, means the same skeptical/quality/verification lens gets applied consistently instead of each stage inventing its own ad hoc review criteria. If a future stage needs a genuinely different mindset, add a new agent file rather than stretching an existing persona to cover it.

Note the difference between `pc-critic` and `pc-reviewer`: `pc-critic` runs *inside* the drafting workflow, *before* anything is committed, and may legitimately report "ready, nothing to fix." `pc-reviewer` runs *after* you've committed a stage's output, standing in for the human gate, and is required to always find something — it's the mechanism that keeps quality pressure on even though no human is looking at every stage.

## Before doing anything: figure out where we are

State lives in the repo, not in this conversation. Re-derive it every time this skill is invoked (context may have been compacted or this may be a fresh session):

1. `git status` — anything uncommitted sitting in `docs/`? That's unreviewed/unapproved work in progress; pick it back up rather than starting fresh.
2. Does `docs/REQUIREMENT.md` exist and is it committed (`git log -- docs/REQUIREMENT.md` has a commit, and `git status` shows no pending changes to it)? If not → **Stage 1**.
3. Does `docs/ARCHITECTURE.md` exist and is it committed? If not → **Stage 2**.
4. Is there at least one `docs/impl/**/IMPLEMENT.md`, committed? If not → **Stage 3**.
5. Inside each `IMPLEMENT.md`, check the phase checklist (see template) and each phase's `STATUS.md`. If any phase isn't fully committed → **Stage 4**, resume at that phase and that phase's first unfinished sub-step.
6. If every phase across every `IMPLEMENT.md` is committed → **Stage 5**.

Tell the user which stage/phase you've detected and what you're about to do before diving in — don't just silently start writing or launching a workflow.

Templates for every document and status file live in `references/templates.md` — the workflow scripts already follow these templates, but read it when you (not a subagent) need to hand-write or patch a file directly.

## The post-commit review gate (used at the end of every stage below)

After you write and commit a stage's artifact, run this fixed sequence before moving on:

1. **Commit** the artifact yourself (`git add` the specific file(s), commit with a message describing the stage). This is the checkpoint — not a request for permission.
2. **Review.** Call the `Agent` tool with `subagent_type: "pc-reviewer"`, pointing it at what was just committed (the stage doc, or for Stage 4 the phase's diff, or for Stage 5 the whole feature) plus the docs that preceded it. It always returns at least one concrete finding.
3. **Save the review.** Write `pc-reviewer`'s full findings to `REVIEW.md` in the relevant phase/stage directory — for Stages 1-2 that's the repo root doc's sibling (there is no per-stage subdirectory, so save it as `docs/REQUIREMENT.REVIEW.md` / `docs/ARCHITECTURE.REVIEW.md`), for Stage 3 and each Stage 4 phase it's that phase's own directory (`docs/impl/<feature-slug>/REVIEW.md` for the plan as a whole, `docs/impl/<feature-slug>/phase-NN-<slug>/REVIEW.md` per phase), for Stage 5 it's `docs/impl/<feature-slug>/FINAL-REVIEW.md`. Include: the findings ranked as returned, and for each one whether/how it was applied (fixed directly, folded into a re-draft, or — rare — recorded as an accepted tradeoff with reasoning). This makes the review durable and readable later without re-running the agent.
4. **Apply.** Fix what it found: small/local fixes, patch the file directly; if a finding reveals the stage's whole approach needs rework, loop back into that stage's drafting workflow with the finding folded into the input, then re-run this sequence from step 1.
5. **Commit the fix** (including `REVIEW.md`) as a separate commit (e.g. "Address pc-reviewer feedback on <stage>").
6. **Advance automatically** to the next stage — do not stop and wait for the user. Only pause if you hit a genuine blocker you can't resolve from the docs/code/pc-reviewer's finding (see the skill description's escape hatch).

## Stage 1 — Requirement

1. Call `Workflow({ scriptPath: ".claude/workflows/requirement-draft.js", args: { repoPath: "<absolute repo path>", featureRequest: "<what the user asked for, in their words plus any context you have>" } })`. It drafts the doc, has an independent subagent critique it, and revises once if the critique found real gaps — you get back `{ markdown, issuesFound }`.
2. Write the returned `markdown` to `docs/REQUIREMENT.md`.
3. Check whether `CLAUDE.md` exists at the repo root.
   - If missing, create a minimal one (or run `/init`) and add a "Docs" section linking `docs/REQUIREMENT.md`.
   - If it exists but doesn't reference `docs/REQUIREMENT.md`, add the link.
   - If it already links there, leave `CLAUDE.md` alone.
4. Run the **post-commit review gate** above against `docs/REQUIREMENT.md`, then continue straight to Stage 2.

## Stage 2 — Architecture

1. Call `Workflow({ scriptPath: ".claude/workflows/architecture-draft.js", args: { repoPath: "<...>", requirementMarkdown: "<contents of docs/REQUIREMENT.md>" } })`. Same draft-critique-revise shape, grounded in real repo code.
2. Write the returned `markdown` to `docs/ARCHITECTURE.md`.
3. Same `CLAUDE.md` link check/add as Stage 1, this time for `docs/ARCHITECTURE.md`.
4. Run the **post-commit review gate** against `docs/ARCHITECTURE.md`, then continue straight to Stage 3.

## Stage 3 — Implementation Plan

1. Decide the feature's doc path: `docs/impl/<feature-slug>/IMPLEMENT.md`. If the feature naturally splits into layers that each need their own breakdown, nest further — `docs/impl/<feature-slug>/<sub-slug>/IMPLEMENT.md` — where each directory segment's name is itself a short summary of what's implemented at that layer. Don't nest for its own sake; most features need exactly one `IMPLEMENT.md`.
2. Call `Workflow({ scriptPath: ".claude/workflows/phase-split.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<contents of docs/ARCHITECTURE.md>" } })`. It decomposes the architecture into phases with a dependency graph, has an independent subagent critique the split, revises once if needed, then fans out one subagent per phase (in parallel) to write that phase's full TDD-ready detail. Returns `{ phases: [{ id, name, deps, touches, summary, detail }, ...] }`.
3. Render `phases` into `IMPLEMENT.md` using the template's phase-checklist + per-phase-section structure. The `deps` array is what later tells you which phases can run in parallel in Stage 4 (no dep relationship + disjoint `touches`).
4. Run the **post-commit review gate** against `IMPLEMENT.md`, then continue straight to Stage 4.

## Stage 4 — Implementation (per phase, TDD)

Group the phases from `IMPLEMENT.md` into **batches**: a batch is a maximal set of not-yet-done phases whose `deps` are all already committed, and that don't declare overlapping `touches`. Process batches in order; within a batch, everything runs together.

For each batch, call `Workflow({ scriptPath: ".claude/workflows/phase-batch-implement.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<...>", phases: [<the batch's phase objects — id, name, detail, touches>], overlappingFiles: false } })`. This runs sub-steps 4-1 through 4-3 for every phase in the batch as independent pipeline chains (no phase waits on another one's chain), each via its own subagent:

- **4-1 Red** — writes a few focused unit test(s) for the phase (core path + the highest-value edge cases, not an exhaustive matrix); expected to fail/not compile yet.
- **4-2 Green** — minimal implementation to make those tests pass, with an actual test run confirming it.
- **4-3 Refactor** — cleans up if warranted, reruns tests, reports if nothing needed changing.

There is no separate Verify sub-step anymore — the `pc-reviewer` gate (4-5 below) covers that job too, to keep the per-phase cycle fast. If any phase's file scope might actually overlap despite looking disjoint on paper, set `overlappingFiles: true` so the workflow isolates each phase's agents in their own git worktree — otherwise leave it off, since worktree isolation is expensive and phases are supposed to be scoped to disjoint files by Stage 3 design.

Once the workflow returns, handle each phase in the batch one at a time:

- **4-4 Commit.** Commit this phase's changes (this is what step 1 of the post-commit review gate calls for) and mark it done in `IMPLEMENT.md`'s phase checklist and the phase's `STATUS.md` (template in `references/templates.md` — track this per phase so you can resume mid-batch after a compaction or new session).
- **4-5 Review gate (covers verify too).** Run the **post-commit review gate**'s steps 2-5 (`pc-reviewer` against this phase's diff, checking spec conformance as well as code quality, apply, commit the fix, then move on) before starting the next phase in the batch. If `pc-reviewer` finds the phase doesn't actually satisfy its spec, treat that like the old 4-4 failure path: re-run just that phase (call the workflow again with only that one phase in `phases`) after addressing the finding, or drop back to Stage 3 if the concern is really a planning gap. Once every phase in the batch is through its own gate, move to the next batch.

## Stage 5 — Final Review

Once every phase in every `IMPLEMENT.md` is committed:

1. Call `Workflow({ scriptPath: ".claude/workflows/final-review.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<...>", phaseResults: [<the accumulated 4-4 verify results across all phases>] } })`. Three independent subagents review in parallel — requirement coverage, code quality, test coverage — each grounded in the real code, not just the plan docs. Returns `{ lensResults: [{ lens, ok, findings }, ...] }`.
2. Synthesize the three lenses into one summary: what's confirmed solid, what each lens flagged, and how it maps back to `docs/REQUIREMENT.md`'s acceptance criteria. Fix anything real the lenses surfaced, committing as you go.
3. Run the **post-commit review gate** one final time against the whole finished feature (`pc-reviewer` reviewing everything, not just the latest commit). Apply its finding(s) and commit.
4. Report the finished cycle to the user — what was built, the final `pc-reviewer` verdict, and any residual notes. The cycle is complete; no further auto-advancement (there is no Stage 6).

## Notes

- If the user asks "지금 어디까지 됐어?" / "무슨 단계야?", just run the state-detection steps above and report — don't start writing anything or launching a workflow.
- If `docs/REQUIREMENT.md` or `docs/ARCHITECTURE.md` need revision *after* later stages already built on them, flag the ripple effect to the user explicitly (which phases/docs downstream may need rework) rather than quietly patching only the one file.
- Every workflow call is a background task — tell the user briefly what's running before you fire it off, the same way you would for any other long-running tool call, and don't fabricate what a workflow "probably" returned before its result actually arrives.
- The user has explicitly authorized autonomous commits through every stage of this cycle (this is what makes the `pc-reviewer` gate work in place of a human one). This authorization is scoped to *committing* — it is not authorization to `push` to any remote; keep pushes to the explicit-request rule from the general operating instructions.
