export const meta = {
  name: 'pc-requirement-draft',
  description: 'project-cycle Stage 1: draft docs/REQUIREMENT.md, then an independent critic checks it for gaps before it goes to the human for review.',
  phases: [
    { title: 'Draft', detail: 'write the requirement doc from the feature request + repo context' },
    { title: 'Critique', detail: 'independent agent looks for gaps, vague acceptance criteria, scope creep' },
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

phase('Draft')
const draft = await agent(
  `You are drafting docs/REQUIREMENT.md for a feature in the repository at ${args.repoPath}.

Feature request from the user:
"""
${args.featureRequest}
"""

First, look at the repo (existing source files, CLAUDE.md if present, any existing docs/) so the requirement is grounded in what actually exists rather than invented context. Do not write any files — just produce the document content.

Write it with exactly these headings: Background, Scope, Non-goals, Constraints, Acceptance Criteria. Acceptance Criteria must be a markdown checklist ("- [ ] ...") where every item is concrete enough that someone could later verify it objectively, since this list is what the final review is graded against.

Return only the full markdown for docs/REQUIREMENT.md.`,
  { phase: 'Draft', agentType: 'pc-analyst', schema: DOC_SCHEMA }
)

phase('Critique')
const critique = await agent(
  `Critique this draft of docs/REQUIREMENT.md for a feature in the repo at ${args.repoPath}. This repo is a C++ Visual Studio project (json_parser) — flag anything that ignores platform/build constraints that matter here.

Look specifically for: acceptance criteria that are vague or unverifiable, missing non-goals (scope that will quietly creep during implementation), missing constraints, and background/scope mismatches. Default to skeptical — if you can find a real gap, report it; don't rubber-stamp.

Draft:
"""
${draft.markdown}
"""

Return ready=true only if you found nothing worth fixing.`,
  { phase: 'Critique', agentType: 'pc-critic', schema: CRITIQUE_SCHEMA }
)

let finalMarkdown = draft.markdown
if (!critique.ready && critique.issues.length) {
  phase('Revise')
  const revised = await agent(
    `Revise this docs/REQUIREMENT.md draft to fix the following issues. Keep everything else that already works intact — don't rewrite from scratch.

Issues to fix:
${critique.issues.map((i) => `- ${i}`).join('\n')}

Original draft:
"""
${draft.markdown}
"""

Return the full revised markdown for docs/REQUIREMENT.md.`,
    { phase: 'Revise', agentType: 'pc-analyst', schema: DOC_SCHEMA }
  )
  finalMarkdown = revised.markdown
}

return { markdown: finalMarkdown, issuesFound: critique.issues }
