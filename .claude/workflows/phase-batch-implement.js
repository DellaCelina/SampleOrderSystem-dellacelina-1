export const meta = {
  name: 'pc-phase-batch-implement',
  description: 'project-cycle Stage 4 sub-steps 4-1..4-3 for a batch of mutually-independent phases: each phase moves through Red -> Green -> Refactor as its own pipeline chain, so no phase waits on another. Verify is folded into the post-commit pc-reviewer gate (sub-step 4-5), which happens outside this workflow along with commit (4-4/4-6), since they need to run after each phase's own commit.',
  phases: [
    { title: 'Red', detail: 'write a few focused failing/uncompiled unit tests per phase' },
    { title: 'Green', detail: 'minimal implementation to pass those tests' },
    { title: 'Refactor', detail: 'clean up, confirm tests still pass' },
  ],
}

// args.phases: array of { id, name, detail, touches, deps } - caller (the skill) is responsible for
// only including phases in one call that have no dependency on each other and don't touch overlapping files.
// args.repoPath, args.requirementMarkdown, args.architectureMarkdown are shared context.

// Workaround: the harness sometimes delivers `args` as a JSON-encoded string rather than the
// parsed object (observed to cause `pipeline() expects an array` when args.phases is read off a
// string). Parse defensively so this script works either way.
const A = typeof args === 'string' ? JSON.parse(args) : args

const isolation = A.overlappingFiles ? 'worktree' : undefined

const results = await pipeline(
  A.phases,

  // 4-1 Red
  (phase) =>
    agent(
      `Write unit test(s) for this implementation phase, in the repo at ${A.repoPath}. It's expected/fine if they fail or don't compile yet - a phase in Red just needs tests that actually exercise the target behavior.

Phase: ${phase.name} (id: ${phase.id})
Detail:
"""
${phase.detail}
"""
Touches: ${phase.touches.join(', ')}

Keep the test list SHORT: cover only the handful of behaviors that matter most for this phase (the
core happy path plus the one or two edge cases most likely to break) - do not try to enumerate
every combination. This is a speed tradeoff the team has explicitly asked for; a lean, well-chosen
test set beats an exhaustive one that slows the cycle down. Find and follow the repo's existing
test setup/conventions before adding new ones. Return a short summary of what tests were added,
where, and their current (expected-red) state.`,
      { phase: 'Red', agentType: 'pc-tester', label: `red:${phase.id}`, isolation }
    ),

  // 4-2 Green
  (redSummary, phase) =>
    agent(
      `Implement this phase in the repo at ${A.repoPath} so the unit tests just written pass. Keep it minimal - don't reach into work that belongs to other phases.

Phase: ${phase.name} (id: ${phase.id})
Detail:
"""
${phase.detail}
"""
Tests added in the Red step:
"""
${redSummary}
"""

Build and actually run the tests to confirm they pass before returning. Return a summary of the implementation plus the real test-run output/result.`,
      { phase: 'Green', agentType: 'pc-implementer', label: `green:${phase.id}`, isolation }
    ),

  // 4-3 Refactor
  (greenSummary, phase) =>
    agent(
      `Review the implementation just added for this phase in ${A.repoPath} and refactor anything worth cleaning up - duplication, naming, structure. Re-run the tests afterward and confirm they're still green. If there's genuinely nothing worth refactoring, say so explicitly rather than making busywork changes.

Phase: ${phase.name} (id: ${phase.id})
What was just implemented:
"""
${greenSummary}
"""

Return a summary of what changed (or confirmation nothing needed to), the post-refactor test result, and a list of every file actually changed for this phase (so the caller can commit and hand this off to review/verify).`,
      { phase: 'Refactor', agentType: 'pc-refactorer', label: `refactor:${phase.id}`, isolation }
    ).then((summary) => ({ summary, phaseId: phase.id, phaseName: phase.name }))
)

return { results: results.filter(Boolean) }
