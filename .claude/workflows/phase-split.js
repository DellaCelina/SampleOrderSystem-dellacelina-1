export const meta = {
  name: 'pc-phase-split',
  description: 'project-cycle Stage 3: decompose an approved ARCHITECTURE.md into an ordered, dependency-aware phase list, critique the split, then expand each phase into full implementation detail in parallel.',
  phases: [
    { title: 'Split', detail: 'decompose architecture into phases + dependency graph' },
    { title: 'Critique', detail: 'independent agent checks the split for missing phases and wrong dependency claims' },
    { title: 'Revise', detail: 'only runs if the critic found real issues' },
    { title: 'Expand', detail: 'one agent per phase writes the detailed TDD-ready spec, in parallel since phases are independent to write about even if not independent to run' },
  ],
}

const SPLIT_SCHEMA = {
  type: 'object',
  properties: {
    phases: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id: { type: 'string' },
          name: { type: 'string' },
          deps: { type: 'array', items: { type: 'string' } },
          touches: { type: 'array', items: { type: 'string' } },
          summary: { type: 'string' },
        },
        required: ['id', 'name', 'deps', 'touches', 'summary'],
      },
    },
  },
  required: ['phases'],
}

const CRITIQUE_SCHEMA = {
  type: 'object',
  properties: {
    ready: { type: 'boolean' },
    issues: { type: 'array', items: { type: 'string' } },
  },
  required: ['ready', 'issues'],
}

const DETAIL_SCHEMA = {
  type: 'object',
  properties: {
    detail: { type: 'string' },
  },
  required: ['detail'],
}

// Workaround: the harness sometimes delivers `args` as a JSON-encoded string rather than the
// parsed object. Parse defensively so this script works either way.
const A = typeof args === 'string' ? JSON.parse(args) : args

phase('Split')
const split = await agent(
  `Decompose this ARCHITECTURE.md into an ordered list of implementation phases for the repo at ${A.repoPath}.

Architecture:
"""
${A.architectureMarkdown}
"""

Requirement it satisfies:
"""
${A.requirementMarkdown}
"""

For each phase give: a short id (e.g. "phase-1"), a short name, its deps (ids of phases it depends on, or empty array if none), the files/modules it touches, and a one-paragraph summary of what it implements. Two phases with disjoint "touches" and no dep relationship between them are candidates for parallel implementation later — make dependency claims honest and minimal (don't over-link phases just because they're related in spirit).`,
  { phase: 'Split', agentType: 'pc-planner', schema: SPLIT_SCHEMA }
)

phase('Critique')
const critique = await agent(
  `Critique this phase split of an architecture for the repo at ${A.repoPath}.

Architecture:
"""
${A.architectureMarkdown}
"""

Proposed phases (JSON):
${JSON.stringify(split.phases, null, 2)}

Check: does every component/flow in the architecture map to some phase (nothing silently dropped)? Are any declared dependencies wrong (either a real dependency is missing, which would break parallel execution, or a fake one is claimed, which would block parallelism that's actually safe)? Is any phase too large to TDD as one unit? Default to skeptical.`,
  { phase: 'Critique', agentType: 'pc-critic', schema: CRITIQUE_SCHEMA }
)

let phases = split.phases
if (!critique.ready && critique.issues.length) {
  phase('Revise')
  const revised = await agent(
    `Revise this phase split to fix the following issues. Keep phases that are already fine unchanged.

Issues:
${critique.issues.map((i) => `- ${i}`).join('\n')}

Original phases (JSON):
${JSON.stringify(split.phases, null, 2)}

Architecture for reference:
"""
${A.architectureMarkdown}
"""`,
    { phase: 'Revise', agentType: 'pc-planner', schema: SPLIT_SCHEMA }
  )
  phases = revised.phases
}

phase('Expand')
const expanded = await parallel(
  phases.map((p) => async () => {
    const result = await agent(
      `Write the full implementation-plan detail for one phase of a feature in the repo at ${A.repoPath}. This text will go directly under this phase's heading in docs/impl/.../IMPLEMENT.md and must be detailed enough that someone doing TDD on it later doesn't need to re-derive design decisions.

Phase: ${p.name} (id: ${p.id})
Summary: ${p.summary}
Touches: ${p.touches.join(', ')}
Depends on: ${p.deps.length ? p.deps.join(', ') : 'none'}

Full architecture for context:
"""
${A.architectureMarkdown}
"""

Cover: the concrete behavior to implement, the unit tests that should exist (including edge cases), and any interface/signature this phase must expose for phases that depend on it. Return only the detail text (no heading, that's added by the caller).`,
      { phase: 'Expand', agentType: 'pc-planner', schema: DETAIL_SCHEMA, label: `expand:${p.id}` }
    )
    return { ...p, detail: result?.detail ?? '' }
  })
)

return { phases: expanded.filter(Boolean) }
