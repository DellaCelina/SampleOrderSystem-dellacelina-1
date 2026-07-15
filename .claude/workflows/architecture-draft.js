export const meta = {
  name: 'pc-architecture-draft',
  description: 'project-cycle Stage 2: draft docs/ARCHITECTURE.md against an approved REQUIREMENT.md, then an independent critic checks coverage and fit with the existing codebase.',
  phases: [
    { title: 'Draft', detail: 'write the architecture doc against the requirement + real repo code' },
    { title: 'Critique', detail: 'independent agent checks every acceptance criterion is covered and the design fits existing code' },
    { title: 'Revise', detail: 'only runs if the critic found real issues' },
  ],
}

const DOC_SCHEMA = {
  type: 'object',
  properties: { markdown: { type: 'string' } },
  required: ['markdown'],
}

const CRITIQUE_SCHEMA = {
  type: 'object',
  properties: {
    ready: { type: 'boolean' },
    issues: { type: 'array', items: { type: 'string' } },
  },
  required: ['ready', 'issues'],
}

// Workaround: the harness sometimes delivers `args` as a JSON-encoded string rather than the
// parsed object. Parse defensively so this script works either way.
const A = typeof args === 'string' ? JSON.parse(args) : args

phase('Draft')
const draft = await agent(
  `You are drafting docs/ARCHITECTURE.md for a feature in the repository at ${A.repoPath}.

The approved requirement (docs/REQUIREMENT.md) is:
"""
${A.requirementMarkdown}
"""

Read the actual existing source files this feature will touch or interact with (this is a C++ project — look at the real headers/cpp files, not just filenames) so the design is grounded in the real code, not guessed structure. Do not write any files — just produce the document content.

Write it with exactly these headings: Overview, Components, Data Flow / Interactions, Key Design Decisions, Open Questions. In Components, name real files/classes in this repo where relevant. Start the doc with a line linking back: "Satisfies: [docs/REQUIREMENT.md](REQUIREMENT.md)".

Return only the full markdown for docs/ARCHITECTURE.md.`,
  { phase: 'Draft', agentType: 'pc-architect', schema: DOC_SCHEMA }
)

phase('Critique')
const critique = await agent(
  `Critique this draft of docs/ARCHITECTURE.md against its requirement doc, for a feature in the repo at ${A.repoPath}.

Requirement (docs/REQUIREMENT.md):
"""
${A.requirementMarkdown}
"""

Architecture draft:
"""
${draft.markdown}
"""

Check specifically: does every acceptance criterion in the requirement have a corresponding component/flow in the architecture? Does the design actually fit the real existing code in this repo, or does it assume structure that doesn't exist? Are there hand-waved "Open Questions" that are actually load-bearing decisions the plan can't proceed without? Default to skeptical.

Return ready=true only if you found nothing worth fixing.`,
  { phase: 'Critique', agentType: 'pc-critic', schema: CRITIQUE_SCHEMA }
)

let finalMarkdown = draft.markdown
if (!critique.ready && critique.issues.length) {
  phase('Revise')
  const revised = await agent(
    `Revise this docs/ARCHITECTURE.md draft to fix the following issues. Keep everything else that already works intact.

Issues to fix:
${critique.issues.map((i) => `- ${i}`).join('\n')}

Original draft:
"""
${draft.markdown}
"""

Return the full revised markdown for docs/ARCHITECTURE.md.`,
    { phase: 'Revise', agentType: 'pc-architect', schema: DOC_SCHEMA }
  )
  finalMarkdown = revised.markdown
}

return { markdown: finalMarkdown, issuesFound: critique.issues }
