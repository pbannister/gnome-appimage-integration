# README for prompts

- `01-contract.md` defines instruction precedence, authority, safety, interaction phases, and output rules.
- `02-workflow.md` defines the execution sequence for tasks.
- `03-conventions.md` defines formatting, naming, repository structure, and file operations.
- `how-to-write-tasks.md` defines the format and rules for human-authored tasks.
- `how-to-write-features.md` defines the format and rules for human-authored features.
- `how-to-write-episodes.md` defines the format and rules for human-authored episodes.
- `common/00-overview.md` describes the purpose of shared prompt files.
- `common/01-requirements.md` defines requirements that apply globally.
- `common/02-universal-rules.md` defines scope, clarification, safety, anti-hallucination, privacy-boundary, risky-operations, and output rules.
- `common/03-glossary.md` defines project terminology and protocol concepts.
- `flavors/01-semantic-sort-naming.md` defines the canonical semantic-sort naming rules.
- `flavors/02-cpp-conventions.md` applies naming and compilation rules when project targets C++.
- `features/` contains feature-specific requirements.
- `tasks/` contains task-specific prompt definitions.
- `episodes/` contains episode work orders.

## Loading Rules

- Always load the authoritative project-rule files listed in `02-workflow.md`.
- The numeric prefixes in the filenames mark the load order defined there.
- Load only feature or task files explicitly referenced by the current task or by a directly referenced dependency.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `01-contract.md`
- `02-workflow.md`
- `03-conventions.md`
- `how-to-write-tasks.md`
- `how-to-write-features.md`
- `how-to-write-episodes.md`
- `common/00-overview.md`
- `common/01-requirements.md`
- `common/02-universal-rules.md`
- `common/03-glossary.md`
- `flavors/01-semantic-sort-naming.md`
- `flavors/02-cpp-conventions.md`
