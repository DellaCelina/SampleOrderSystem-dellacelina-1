export const meta = {
  name: 'pc-phase-batch-implement',
  description: 'project-cycle Stage 4 sub-steps 4-1..4-4 for a batch of mutually-independent phases: each phase moves through Red -> Green -> Refactor -> Verify as its own pipeline chain, so no phase waits on another. Sub-steps 4-5 (human review) and 4-6 (commit) happen outside this workflow, since they need a human in the loop.',
  phases: [
    { title: 'Red', detail: 'write failing/uncompiled unit tests per phase' },
    { title: 'Green', detail: 'minimal implementation to pass those tests' },
    { title: 'Refactor', detail: 'clean up, confirm tests still pass' },
    { title: 'Verify', detail: 'check behavior against requirement/architecture/phase spec, not just green tests' },
  ],
}

const VERIFY_SCHEMA = {
  type: 'object',
  properties: {
    passed: { type: 'boolean' },
    summary: { type: 'string' },
    filesChanged: { type: 'array', items: { type: 'string' } },
    concerns: { type: 'array', items: { type: 'string' } },
  },
  required: ['passed', 'summary', 'filesChanged', 'concerns'],
}

// args.phases: array of { id, name, detail, touches, deps } — caller (the skill) is responsible for
// only including phases in one call that have no dependency on each other and don't touch overlapping files.
// args.repoPath, args.requirementMarkdown, args.architectureMarkdown are shared context.

const isolation = args.overlappingFiles ? 'worktree' : undefined

const results = await pipeline(
  args.phases,

  // 4-1 Red
  (phase) =>
    agent(
      `Write unit test(s) for this implementation phase, in the repo at ${args.repoPath}. It's expected/fine if they fail or don't compile yet — a phase in Red just needs tests that actually exercise the target behavior.

Phase: ${phase.name} (id: ${phase.id})
Detail:
"""
${phase.detail}
"""
Touches: ${phase.touches.join(', ')}

Find and follow the repo's existing test setup/conventions before adding new ones. Return a short summary of what tests were added, where, and their current (expected-red) state.`,
      { phase: 'Red', agentType: 'pc-tester', label: `red:${phase.id}`, isolation }
    ),

  // 4-2 Green
  (redSummary, phase) =>
    agent(
      `Implement this phase in the repo at ${args.repoPath} so the unit tests just written pass. Keep it minimal — don't reach into work that belongs to other phases.

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
      `Review the implementation just added for this phase in ${args.repoPath} and refactor anything worth cleaning up — duplication, naming, structure. Re-run the tests afterward and confirm they're still green. If there's genuinely nothing worth refactoring, say so explicitly rather than making busywork changes.

Phase: ${phase.name} (id: ${phase.id})
What was just implemented:
"""
${greenSummary}
"""

Return a summary of what changed (or confirmation nothing needed to) and the post-refactor test result.`,
      { phase: 'Refactor', agentType: 'pc-refactorer', label: `refactor:${phase.id}`, isolation }
    ),

  // 4-4 Verify
  (refactorSummary, phase) =>
    agent(
      `Verify that this phase actually satisfies its intent — not just "tests are green," but that the behavior matches the requirement, architecture, and this phase's own spec. If something is off, say so concretely (don't just say "looks fine").

Phase: ${phase.name} (id: ${phase.id})
Phase detail/spec:
"""
${phase.detail}
"""
Requirement doc:
"""
${args.requirementMarkdown}
"""
Architecture doc:
"""
${args.architectureMarkdown}
"""
Work done so far (post-refactor):
"""
${refactorSummary}
"""
Repo: ${args.repoPath}

List every file actually changed for this phase.`,
      { phase: 'Verify', agentType: 'pc-verifier', label: `verify:${phase.id}`, schema: VERIFY_SCHEMA, isolation }
    ).then((v) => ({ ...v, phaseId: phase.id, phaseName: phase.name }))
)

return { results: results.filter(Boolean) }
