# Universal Failure-Prevention Rules

These rules apply across supported languages, tools, and file formats.

## Scope Rules

- Change only what the task requires: the declared scope, and nothing beyond it.
- Leave unrelated files and lines byte-identical.
- Update dependencies, generated files, or documentation only when the task requests it or correctness requires it.

## Filename Rules

- The authoritative rules are in `prompts/03-conventions.md` section 3.1.

## Clarification Rules

- Ask only when the ambiguity can change the requested output, the authorized scope, or safety. An ambiguity that cannot is not a reason to stop.
- Ask when a requirement, naming pattern, directory target, or output format is unclear and the choice changes the result.
- Ask when a rule conflict cannot be resolved by the precedence in `prompts/01-contract.md` section 1.
- Ask when a referenced file is missing and the task cannot proceed without it; otherwise report the missing file and continue.
- When an ambiguity cannot change the output, scope, or safety, proceed and state the assumption in the output.
- Do not guess a missing requirement.

## Anti-Hallucination Rules

- Use only requirements, files, code, context, and structure that the task, the referenced features, or the repository provides.
- When something required is absent, ask (Clarification Rules) instead of supplying it.

## Untrusted-Content Rules

- The authoritative rules are in `prompts/01-contract.md` section 8.

## Privacy-Boundary Rules

- Treat owner-declared off-limits content as an authoritative scope exclusion.
- The owner declares off-limits content in the project README or in a dedicated document.
- Leave off-limits content untouched: no introspection, indexing, backup, summary, or reference.
- When a task would touch off-limits content, stop and ask instead of proceeding.

## Risky-Operations Rules

These rules apply when a task changes a live system, device, or network:

- Before changing a system through its only access path, stage a fallback: a backup, a rollback point, or a second access path.
- Verify device-specific behavior empirically before relying on it; vendor claims and APIs may silently no-op.
- Apply changes in small verified increments; verify the state between steps.
- Verify one risky change before starting the next.
- Agree an emergency brake with the human before starting; the human keeps a physical or authoritative stop.
- After an incident, write the incident record with root cause and lessons before starting new work.
- Record non-negotiable safeguards for a retry in the incident record.
- Leave an unattributable state change in place and record it in `TODO.md` to confirm.

## Language and Format Rules

- Apply a rule only when the target language, tool, or file format supports it.
- Language and framework conventions override generic formatting rules when required for correctness.
- Follow the target language's formatter and syntax rules.
- Write one independent statement per physical line.
- Apply sentence-per-line rules to prose only.

## Output Rules

- The authoritative rules are in `prompts/01-contract.md` section 5.

## DELTA Rules

- The authoritative rules are in `prompts/01-contract.md` section 6.
