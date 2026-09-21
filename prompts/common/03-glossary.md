# Glossary

## Semantic-sort naming

- The complete semantic-sort naming rules are defined in `prompts/flavors/01-semantic-sort-naming.md`.

## DELTA protocol

- DELTA applies minimal corrections.
- A DELTA applies to the immediately preceding assistant output unless another artifact is identified.
- A DELTA changes only the named portions.
- A DELTA does not regenerate full output unless explicitly requested.
- If a DELTA cannot be applied without changing additional portions, clarification is required.

## Universal failure-prevention rules

- Universal failure-prevention rules apply across supported languages, tools, and file formats.
- These rules define scope, clarification, anti-hallucination, safety, privacy-boundary, risky-operations, and output behavior.

## One-sentence-per-line

- Each prose sentence occupies one line when the applicable document convention requires it.
- This rule does not apply to code blocks.

## One-statement-per-line

- Independent statements must not be combined on one physical line when the target language supports separating them.
- The target language formatter and syntax rules take precedence.

## Feature file

- A file in `prompts/features/`.
- A feature file defines a project capability.
- A feature file describes requirements and behavior.
- A feature file applies only when referenced by the task or a directly referenced feature dependency.

## Plan block

- A block listing ordered steps.

## Output block

- A block containing final output.
- An output block follows the exact requested format.
- An output block contains no unrequested commentary.

## Context block

- A block providing additional information.
- A context block contains file contents, requirements, notes, samples, or constraints.
- A context block does not add instructions unless explicitly labeled as a constraint.

## Scope-based identifier length

- The amount of identifier detail appropriate to the identifier's scope.

## Ordered transformation pipeline

- A sequence of transformations applied in a defined order.

## Generated file

- A generated file is produced by a script, build tool, generator, or other automated process.
- Generated files must be identified as generated.
- Generated files must not be edited manually unless explicitly requested.
- Generated output must be written only to the designated output directory.

## Incident record

- A record of an incident: what happened, root cause, fix, lessons, and safeguards for a retry.
- An incident record is written after the incident settles and before new work starts.

## Handoff record

- A record written at a session boundary so a fresh session resumes without the prior conversation's memory.
- A handoff record lists the verified current state and the next tasks in order.

## Live-state test

- A test that verifies a documented model against live reality.
- A live-state test requires declared access and reports PASS/WARN/FAIL.
- Live-state tests form one tier of the test policy in `prompts/02-workflow.md`.

## Privacy boundary

- Owner-declared content that is off-limits to the LLM.
- Privacy boundaries are authoritative scope exclusions.

## Stability rules

- Stability rules define project behavior that should not change without explicit instruction.
