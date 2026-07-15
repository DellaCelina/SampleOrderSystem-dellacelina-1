---
name: project-cycle
description: Run this project's gated development lifecycle as a dynamic multi-agent workflow — Requirement → Architecture → Implementation Plan → TDD Implementation (per phase, parallel where possible) → Final Review. Each stage's document is drafted and critiqued by subagents via the Workflow tool, but every stage boundary is still a human gate; never auto-advance past one. Use whenever the user asks to start/continue a new feature in this repo, says things like "다음 단계로 진행해줘", "요구사항 작성해줘", "phase 구현 시작해줘", or asks "지금 어느 단계야?". Always check current state from the repo (docs/ + git) before acting — never assume the stage from conversation memory alone.
---

# Project Cycle

A fixed 5-stage lifecycle for building a feature in this repo. The user has explicitly asked for this to run as a **dynamic workflow with subagents**, not a single agent writing documents alone — so each stage's actual drafting/implementation work is delegated to a `Workflow` script under `.claude/workflows/` that fans out to subagents (a drafter + an independent critic, parallel phase writers, parallel TDD chains, parallel review lenses). That user request is the standing authorization to invoke these workflows for this skill; you don't need to re-ask each time.

What stays with you (the skill invocation), never with the workflow scripts: **the human gates**. A `Workflow` run executes to completion in the background without stopping for approval, so review-and-commit checkpoints cannot live inside the script — they live in the orchestration below, between workflow calls. Never let a workflow's output get committed without the user actually reviewing it first.

## The agent roster

Every `agent()` call in these workflows uses a purpose-built subagent type from `.claude/agents/`, not generic `general-purpose` — each has a scoped toolset and a persona tuned to one job, so quality doesn't depend on re-explaining the role in every prompt, and read-only roles (analyst, architect, planner, critic, verifier) can't quietly patch around a problem instead of reporting it:

| Agent | Role | Tools | Used in |
|---|---|---|---|
| `pc-analyst` | 기획자 — turns a request into a checkable requirement | read-only | Stage 1 draft/revise |
| `pc-architect` | 설계자 — designs against the real codebase | read-only | Stage 2 draft/revise |
| `pc-planner` | 계획자 — splits architecture into dependency-aware phases | read-only | Stage 3 split/revise/expand |
| `pc-critic` | 비평가 — adversarial pre-review of a draft doc, defaults to skeptical | read-only | Stage 1/2/3 critique steps |
| `pc-tester` | 테스터 — writes Red-state tests; reviews test coverage in final review | full | Stage 4 (4-1), Stage 5 test-coverage lens |
| `pc-implementer` | 구현자 — minimal Green implementation, stays in phase scope | full | Stage 4 (4-2) |
| `pc-refactorer` | 리팩터러 — cleans up without changing behavior; reviews code quality in final review | full | Stage 4 (4-3), Stage 5 code-quality lens |
| `pc-verifier` | 검증자 — checks behavior against spec, can't edit so its verdict stays honest | read-only + Bash | Stage 4 (4-4), Stage 5 requirement-coverage lens |

Reuse is deliberate, not laziness: `pc-critic` reviewing three different doc types, or `pc-tester`/`pc-refactorer`/`pc-verifier` doing double duty in final review, means the same skeptical/quality/verification lens gets applied consistently instead of each stage inventing its own ad hoc review criteria. If a future stage needs a genuinely different mindset, add a new agent file rather than stretching an existing persona to cover it.

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

## Stage 1 — Requirement

1. Call `Workflow({ scriptPath: ".claude/workflows/requirement-draft.js", args: { repoPath: "<absolute repo path>", featureRequest: "<what the user asked for, in their words plus any context you have>" } })`. It drafts the doc, has an independent subagent critique it, and revises once if the critique found real gaps — you get back `{ markdown, issuesFound }`.
2. Write the returned `markdown` to `docs/REQUIREMENT.md`.
3. Check whether `CLAUDE.md` exists at the repo root.
   - If missing, create a minimal one (or run `/init`) and add a "Docs" section linking `docs/REQUIREMENT.md`.
   - If it exists but doesn't reference `docs/REQUIREMENT.md`, add the link.
   - If it already links there, leave `CLAUDE.md` alone.
4. **Stop.** Tell the user the requirement doc is ready for review (mention `issuesFound` if the critic caught anything worth their attention even after the revision). Do not commit it yourself — the user reviews, commits, and tells you to continue.
5. Only proceed to Stage 2 once the user explicitly says so (e.g. "다음으로 진행"). If they instead give feedback, either patch `docs/REQUIREMENT.md` directly for small edits, or re-run the workflow with the feedback folded into `featureRequest` for a substantial rewrite, then return to step 4.

## Stage 2 — Architecture

1. Call `Workflow({ scriptPath: ".claude/workflows/architecture-draft.js", args: { repoPath: "<...>", requirementMarkdown: "<contents of docs/REQUIREMENT.md>" } })`. Same draft-critique-revise shape, grounded in real repo code.
2. Write the returned `markdown` to `docs/ARCHITECTURE.md`.
3. Same `CLAUDE.md` link check/add as Stage 1, this time for `docs/ARCHITECTURE.md`.
4. **Stop** for review. Same rule: user reviews + commits + tells you to continue, or gives feedback and you revise (patch directly or re-run the workflow) and return to this step.

## Stage 3 — Implementation Plan

1. Decide the feature's doc path: `docs/impl/<feature-slug>/IMPLEMENT.md`. If the feature naturally splits into layers that each need their own breakdown, nest further — `docs/impl/<feature-slug>/<sub-slug>/IMPLEMENT.md` — where each directory segment's name is itself a short summary of what's implemented at that layer. Don't nest for its own sake; most features need exactly one `IMPLEMENT.md`.
2. Call `Workflow({ scriptPath: ".claude/workflows/phase-split.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<contents of docs/ARCHITECTURE.md>" } })`. It decomposes the architecture into phases with a dependency graph, has an independent subagent critique the split, revises once if needed, then fans out one subagent per phase (in parallel) to write that phase's full TDD-ready detail. Returns `{ phases: [{ id, name, deps, touches, summary, detail }, ...] }`.
3. Render `phases` into `IMPLEMENT.md` using the template's phase-checklist + per-phase-section structure. The `deps` array is what later tells you which phases can run in parallel in Stage 4 (no dep relationship + disjoint `touches`).
4. **Stop** for review, same gate rule as above.

## Stage 4 — Implementation (per phase, TDD)

Group the phases from `IMPLEMENT.md` into **batches**: a batch is a maximal set of not-yet-done phases whose `deps` are all already committed, and that don't declare overlapping `touches`. Process batches in order; within a batch, everything runs together.

For each batch, call `Workflow({ scriptPath: ".claude/workflows/phase-batch-implement.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<...>", phases: [<the batch's phase objects — id, name, detail, touches>], overlappingFiles: false } })`. This runs sub-steps 4-1 through 4-4 for every phase in the batch as independent pipeline chains (no phase waits on another one's chain), each via its own subagent:

- **4-1 Red** — writes unit test(s) for the phase; expected to fail/not compile yet.
- **4-2 Green** — minimal implementation to make those tests pass, with an actual test run confirming it.
- **4-3 Refactor** — cleans up if warranted, reruns tests, reports if nothing needed changing.
- **4-4 Verify** — checks the phase against the requirement/architecture/phase spec, not just green tests; returns `{ passed, summary, filesChanged, concerns }` per phase.

If any phase's file scope might actually overlap despite looking disjoint on paper, set `overlappingFiles: true` so the workflow isolates each phase's agents in their own git worktree — otherwise leave it off, since worktree isolation is expensive and phases are supposed to be scoped to disjoint files by Stage 3 design.

Once the workflow returns, handle each phase in the batch (still one at a time for the human-facing steps — these can't be parallelized away):

- **4-4 fallback.** If a phase's `passed` came back `false`, don't push it to the user — go back and re-run just that phase (you can call the workflow again with only that one phase in `phases`) after addressing `concerns`, or drop back to Stage 3 if the concern is really a planning gap.
- **4-5 Review.** Show the user that phase's `summary`/`filesChanged`/diff. If approved, continue to 4-6. If not, re-run the phase (or the relevant earlier stage) with their feedback folded in.
- **4-6 Commit.** Once approved, commit this phase's changes yourself (you may commit — this is a within-stage checkpoint, not a stage-boundary gate) and mark it done in `IMPLEMENT.md`'s phase checklist and the phase's `STATUS.md` (template in `references/templates.md` — track this per phase so you can resume mid-batch after a compaction or new session). Move to the next phase in the batch, then the next batch.

Note the asymmetry with Stages 1-3: those gates end with the *user* committing after reviewing a whole document. Here, since phases are numerous and incremental, you commit each phase yourself once the user has approved *that phase's* review — this keeps the loop tight instead of stacking up multiple uncommitted phases waiting on one big review.

## Stage 5 — Final Review

Once every phase in every `IMPLEMENT.md` is committed:

1. Call `Workflow({ scriptPath: ".claude/workflows/final-review.js", args: { repoPath: "<...>", requirementMarkdown: "<...>", architectureMarkdown: "<...>", phaseResults: [<the accumulated 4-4 verify results across all phases>] } })`. Three independent subagents review in parallel — requirement coverage, code quality, test coverage — each grounded in the real code, not just the plan docs. Returns `{ lensResults: [{ lens, ok, findings }, ...] }`.
2. Synthesize the three lenses into one summary for the user: what's confirmed solid, what each lens flagged, and how it maps back to `docs/REQUIREMENT.md`'s acceptance criteria.
3. Ask the user for a final holistic review.
4. If they raise issues, route back to the specific stage/phase that owns the problem (don't default to "redo everything" or re-running the whole final-review workflow blindly). If they approve, the workflow is complete — no further action needed.

## Notes

- If the user asks "지금 어디까지 됐어?" / "무슨 단계야?", just run the state-detection steps above and report — don't start writing anything or launching a workflow.
- If `docs/REQUIREMENT.md` or `docs/ARCHITECTURE.md` need revision *after* later stages already built on them, flag the ripple effect to the user explicitly (which phases/docs downstream may need rework) rather than quietly patching only the one file.
- Every workflow call is a background task — tell the user briefly what's running before you fire it off, the same way you would for any other long-running tool call, and don't fabricate what a workflow "probably" returned before its result actually arrives.
