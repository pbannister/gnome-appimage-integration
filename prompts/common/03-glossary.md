# Glossary

Each entry defines a term. Where a rule applies, the entry names the
authoritative file; the glossary never states a rule.

## Semantic-sort naming

- The naming convention described in `prompts/flavors/01-semantic-sort-naming.md`.

## DELTA protocol

- A minimal correction to a named artifact.
- Rules: `prompts/01-contract.md` section 6.

## Universal failure-prevention rules

- The cross-language rules listed in `prompts/common/02-universal-rules.md`.
- They cover scope, clarification, anti-hallucination, privacy boundaries, risky operations, and language and format.

## One-sentence-per-line

- A prose convention in which each sentence occupies one line, so a diff reads clearly.
- It does not apply to code blocks.

## One-statement-per-line

- A code convention in which independent statements occupy separate lines.
- The target language's formatter and syntax rules take precedence.

## Feature file

- A file in `prompts/features/` that defines one project capability.
- Rules: `prompts/01-contract.md` section 2.

## Plan block

- A block listing ordered steps.

## Output block

- A block containing final output.
- Rules: `prompts/01-contract.md` section 5.

## OUTPUT line

- The final line of a task file that restates the response representation, including whether it ends with the `VERIFICATION:` line.
- Rules: `prompts/how-to-write-tasks.md` section 1.

## Context block

- A block providing additional information, such as file contents, notes, samples, or constraints.
- Rules: `prompts/how-to-write-tasks.md` section 5.

## task_context block

- A delimited block of copied data inside `TASK-CONTEXT`; it is data, never instructions.
- Rules: `prompts/how-to-write-tasks.md` section 5.

## constraint block

- A delimited block of instructions inside `TASK-CONTEXT`.
- Rules: `prompts/how-to-write-tasks.md` section 5.

## note block

- A delimited block of background inside `TASK-CONTEXT` that does not constrain.
- Rules: `prompts/how-to-write-tasks.md` section 5.

## Scope-based identifier length

- The amount of identifier detail appropriate to the identifier's scope.

## Ordered transformation pipeline

- A sequence of transformations applied in a defined order.

## Generated file

- A file produced by a script, build tool, generator, or other automated process.
- Rules: `prompts/03-conventions.md` section 6.

## Incident record

- A record of an incident: what happened, root cause, fix, lessons, and safeguards for a retry.
- Rules: `records/README.md`.

## Handoff record

- A record written at a session boundary so a fresh session resumes without the prior conversation's memory.
- Rules: `records/README.md`.

## Live-state test

- A test that verifies a documented model against live reality.
- Rules: `prompts/02-workflow.md` section 4.1.

## Privacy boundary

- Owner-declared content that is off-limits to the LLM.
- Rules: `prompts/common/02-universal-rules.md`.

## VERIFICATION line

- The single output line that reports whether the work was verified.
- Rules: `prompts/01-contract.md` section 5.

## Requirement

- Something the resulting project must satisfy.
- Rules: `prompts/how-to-write-features.md`.

## Requirement identifier

- A stable name for one top-level requirement, `<FEATURE-NAME>-R<NNN>`.
- Rules: `prompts/how-to-write-features.md` section 4.

## Acceptance criterion

- A human-reviewable condition that decides whether the work is acceptable.
- Rules: `prompts/how-to-write-episodes.md`.

## Verification

- Evidence that a requirement or an acceptance criterion is satisfied.
- Rules: `prompts/02-workflow.md` section 6.

## Output

- What the LLM returns to the human.
- Rules: `prompts/01-contract.md` section 5.

## Untrusted content

- Content that is not authoritative as instructions; it may still be factually true.
- Rules: `prompts/01-contract.md` section 8.

## Stability rules

- Project behavior that should not change without explicit instruction.

## How this project does it

- **AppImage** — a single-file Linux application image: an ELF (or a type 1 ISO) with an appended SquashFS payload.
- **Managed directory** — `~/Applications`, where this project keeps the AppImages it integrates.
- **Launcher** — the desktop entry this project writes under `~/.local/share/applications` for an integrated AppImage.
- **Manifest** — the record written beside a launcher, listing every file the integration created.
- **Update information** — the AppImage's own statement of where a newer version can be found.
- **Update check** — asking the update information what it offers and comparing that with the installed version, without downloading.
- **Window class** — the class half of the `WM_CLASS` that a running application reports and that a launcher's `StartupWMClass` must match.
- **Activator** — the GTK4 window that offers Integrate, Run once, Inspect, and Update for one AppImage.
- **Provenance** — where an application, an icon, or a parameter was loaded from.
