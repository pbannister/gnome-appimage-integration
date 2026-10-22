# README for prompts

- `01-contract.md` defines instruction precedence, authority, safety, interaction phases, and output rules.
- `02-workflow.md` defines the execution sequence for tasks.
- `03-conventions.md` defines formatting, naming, repository structure, and file operations.
- `how-to-write-tasks.md` defines the format and rules for human-authored tasks.
- `how-to-write-features.md` defines the format and rules for human-authored features.
- `how-to-write-episodes.md` defines the format and rules for human-authored episodes.
- `how-to-write-research.md` defines the format and rules for a research or study series and its decision records.
- `common/01-universal-rules.md` defines scope, clarification, safety, anti-hallucination, privacy-boundary, risky-operations, and output rules.
- `common/03-glossary.md` defines project terminology and protocol concepts; it is a reference, loaded when a term's meaning decides the work.
- `flavors/01-semantic-sort-naming.md` defines the canonical semantic-sort naming rules.
- `flavors/02-cpp-conventions.md` applies naming and compilation rules when project targets C++.
- `capabilities/` contains the optional capability conventions, loaded only for a task that touches one.
- `features/` contains feature-specific requirements.
- `tasks/` contains task-specific prompt definitions.
- `episodes/` contains episode work orders.

## Loading Rules

- Always load the authoritative project-rule files listed in `02-workflow.md`.
- The numeric prefixes in the filenames mark the load order defined there.
- Load only feature or task files explicitly referenced by the current task or by a directly referenced dependency.
- Load the capability conventions, the glossary, and tool rules only when the task needs them.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `01-contract.md`
- `02-workflow.md`
- `03-conventions.md`
- `how-to-write-tasks.md`
- `how-to-write-features.md`
- `how-to-write-episodes.md`
- `how-to-write-research.md`
- `common/01-universal-rules.md`
- `common/03-glossary.md`
- `flavors/01-semantic-sort-naming.md`
- `flavors/02-cpp-conventions.md`
- `capabilities/01-generated-documentation.md`
- `capabilities/02-version-information.md`
- `capabilities/03-generated-data-products.md`
- `capabilities/04-release-and-publishing.md`
- `capabilities/05-pinned-dependencies.md`
- `capabilities/06-execution-boundary.md`
- `capabilities/07-session-log.md`
- `capabilities/08-delegation-authority.md`
- `capabilities/09-failure-attribution.md`
