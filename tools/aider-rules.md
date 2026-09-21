# Aider Integration Rules

These rules apply only when the model is used via Aider.
These rules constrain Aider's editing behavior.
These rules do not apply to other tools or direct model use.
This file is loaded by Aider at launch through `.aider.conf.yml`.

## Authority

Aider must follow prompts/01-contract.md.
Aider must follow prompts/02-workflow.md.
Aider must follow prompts/03-conventions.md.
Aider must follow prompts/common/02-universal-rules.md.

## File Boundary Enforcement

Aider must modify only the files explicitly listed in the task.
Aider must not modify any file not listed in the task.
Aider must not propose edits to unlisted files.
Aider must not include unlisted files in diffs.
Aider must not scan unlisted files for changes.
Aider must not infer additional files from context.

## File Creation Safety

Aider must not create new files unless explicitly instructed.
Aider must not propose new files unless explicitly instructed.
Aider must not infer new filenames.
Aider must not infer new directories.
Aider must not generate placeholder files.

## File Rename Safety

Aider must not rename files unless explicitly instructed.
Aider must not propose renames.
Aider must not infer renames from naming patterns.
Aider must not reorganize directories.

## File Deletion Safety

Aider must not delete files unless explicitly instructed.
Aider must not propose deletions.
Aider must not infer deletions from refactors.

## Diff Stability

Aider must produce minimal diffs.
Aider must not rewrite entire files unless explicitly instructed.
Aider must not reorder lines unless explicitly instructed.
Aider must not collapse multiple statements into one line.
Aider must not collapse multiple sentences into one line.
Aider must not reformat unrelated sections.

## Semantic-Sort Naming Safety

Aider must follow semantic-sort naming exactly.
Aider must not reorder semantic-sort components.
Aider must not shorten semantic-sort names.
Aider must not expand semantic-sort names.
Aider must not infer naming patterns.

## Plan Mode Safety

Aider must follow prompts/02-workflow.md when generating plans.
Aider must not invent plan steps.
Aider must not skip plan steps.
Aider must not merge plan steps.
Aider must not reorder plan steps.

## Verification

Aider must run `make test` before reporting completion.
Aider must not claim tests passed unless `make test` succeeded.
Aider must report when verification could not be performed.

## Commit Safety

Aider must not auto-commit partial or unverified work.
Aider must commit completed work only after verification succeeds.
Aider must create one commit per completed task.
Aider must not commit generated output, logs, or unrelated files.

## Version Control and Error Recovery

Aider must rely on Git and native rollback commands (such as /undo) for reverting edits.
Aider must not enforce text-based DELTA syntax during interactive Aider sessions.

## Clarification Enforcement

Aider must ask when uncertain.
Aider must ask when naming patterns are unclear.
Aider must ask when directory targets are unclear.
Aider must ask when output format is unclear.
Aider must ask when file boundaries are unclear.

## Stability

These rules are stable.
These rules are authoritative.
These rules must be followed for all Aider-assisted edits.
