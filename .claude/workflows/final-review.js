export const meta = {
  name: 'pc-final-review',
  description: 'project-cycle Stage 5: three independent lenses (requirement coverage, code quality, test coverage) review the whole completed feature in parallel, so the human gets a synthesized picture instead of one pass trying to cover everything.',
  phases: [{ title: 'Review' }],
}

const LENS_SCHEMA = {
  type: 'object',
  properties: {
    ok: { type: 'boolean' },
    findings: { type: 'array', items: { type: 'string' } },
  },
  required: ['ok', 'findings'],
}

const LENSES = [
  {
    key: 'requirement-coverage',
    agentType: 'pc-verifier',
    prompt: (a) =>
      `Check whether every acceptance criterion in docs/REQUIREMENT.md is actually satisfied by the finished feature in the repo at ${a.repoPath}. Go read the real code, don't infer from the plan docs alone.

Requirement:
"""
${a.requirementMarkdown}
"""

Phase summaries from implementation:
${JSON.stringify(a.phaseResults, null, 2)}

Report ok=false if any acceptance criterion is unmet, only partially met, or you can't confirm it from the actual code.`,
  },
  {
    key: 'code-quality',
    agentType: 'pc-refactorer',
    prompt: (a) =>
      `Review the code changes for this feature in the repo at ${a.repoPath} for quality: naming, duplication across phases that should have been shared, structure, and fit with the rest of the codebase's existing style.

Architecture (for intended structure):
"""
${a.architectureMarkdown}
"""

Phase summaries:
${JSON.stringify(a.phaseResults, null, 2)}`,
  },
  {
    key: 'test-coverage',
    agentType: 'pc-tester',
    prompt: (a) =>
      `Review test coverage for this feature in the repo at ${a.repoPath}. For each phase, check the tests actually cover the edge cases called for in that phase's spec, not just the happy path. Run the full test suite if possible and report the real result.

Phase summaries:
${JSON.stringify(a.phaseResults, null, 2)}`,
  },
]

phase('Review')
const lensResults = await parallel(
  LENSES.map((lens) => async () => {
    const result = await agent(lens.prompt(args), {
      phase: 'Review',
      agentType: lens.agentType,
      label: lens.key,
      schema: LENS_SCHEMA,
    })
    return { lens: lens.key, ...result }
  })
)

return { lensResults: lensResults.filter(Boolean) }
