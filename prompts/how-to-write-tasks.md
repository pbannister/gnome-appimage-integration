# How to Write Tasks

This document defines how a human must write tasks for the LLM.

Apply naming rules from `prompts/flavors/01-semantic-sort-naming.md`.

## 1. Task Structure

Follow the applicable project rules for all task-writing requirements.

Every task must contain these sections in order:

* TASK-DESCRIPTION
A clear description of the requested work.
* TASK-OUTPUT
A precise description of the response representation.
* (Optional) TASK-CONTEXT
Additional information, requirements, notes, constraints, or file contents.
* TASK-FILES
The exact operations and paths in scope, as a table.
* TASK-VERIFY
The verification to run and its expected result.
* (When the task applies feature requirements) TASK-FEATURES
The feature files whose requirements the task applies.
* (When the task applies feature requirements) TASK-ACCEPTANCE
The requirement identifiers the task satisfies.

The task file ends with one `OUTPUT:` line that restates the response representation in a sentence, including whether the response ends with the `VERIFICATION:` line. It is the last line the LLM reads, which keeps the format requirement in recent context.

The phrase `Execute the next TODO task` selects the first unchecked `TODO.md` item; the LLM drafts a conforming task from it and the human ratifies it before execution.

## 1.1 Task and Feature Workflow

* Use a feature for a project capability or stable behavioral requirement.
    * Feature files define requirements.
    * Task files define executable work.

* Use a task for one bounded unit of work against the repository.
    * Long-form task definitions belong in `prompts/tasks/` and must follow this document.
    * A task must reference applicable feature files explicitly.
    * A task may create, modify, delete, rename, or inspect files only when those operations are stated in its `TASK-DESCRIPTION` section.

## 2. Section Meaning

* TASK-DESCRIPTION defines the requested work.
* TASK-OUTPUT defines the response representation.
* TASK-CONTEXT provides information: a `<constraint>` block is an instruction, a `<task_context>` block is data, and unlabeled content is background.
* TASK-FILES declares the exact operations and paths in scope as a table; the operation is authorized by `TASK-DESCRIPTION` or by the workflow (see `prompts/01-contract.md` section 3).
* TASK-VERIFY declares the verification to run and its expected result.
* TASK-FEATURES lists the feature files whose requirements the task applies.
* TASK-ACCEPTANCE lists the requirement identifiers, from the feature files in `TASK-FEATURES`, that the task satisfies.
* File scope comes only from `TASK-DESCRIPTION` and `TASK-FILES`; a referenced feature adds requirements, not scope (see `prompts/how-to-write-features.md` section 5).

## 3. Writing the TASK-DESCRIPTION Section

* The TASK-DESCRIPTION section must:
    * describe the goal clearly.
    * avoid ambiguity and unstated assumptions.
    * state each file operation on its own line as `<Verb>: \`path\``, where `<Verb>` is `Create`, `Modify`, `Delete`, `Rename`, or `Inspect`.
    * For `Rename`, name both paths: `- Rename: \`old/path\` → \`new/path\``.
    * Use repository-relative paths, in backticks.
    * explain the goal in the prose around the operation lines.
    * Do not identify a file only by its purpose, role, or directory.

* The TASK-DESCRIPTION section must not:
    * mix implementation instructions with output requirements.

The operation lines are the machine-checkable statement of scope; `TASK-FILES` must agree with them (section 6).

Example:
```markdown
## TASK-DESCRIPTION
- Create: `scripts/site-build.sh`
The script generates `site.out/` from `site.in/`.
- Create: `site.in/hello.txt`
```

## 4. Writing the TASK-OUTPUT Section

TASK-OUTPUT must be explicit.

* It must specify the required files when file contents are requested.
* It must specify ordering when multiple files are required.
* It must specify whether commentary is allowed.
* Commentary is not allowed unless explicitly requested.
* It must specify whether filenames are included.
* It must state whether the response ends with the `VERIFICATION:` line. The default is that it does; a format that omits the line says so.

End the task file with the one-line `OUTPUT:` restatement (section 1).

Examples:
```markdown
## TASK-OUTPUT
Provide only the complete content of `scripts/site-build.sh`, then the `VERIFICATION:` line.
```
```markdown
## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/auth/auth_handler.cpp`
2. `sources/auth/auth_handler.h`
```
```markdown
## TASK-OUTPUT
Provide a semantic-sort plan followed by the complete requested file content, then the `VERIFICATION:` line.
```

## 5. Writing the TASK-CONTEXT Section

Use TASK-CONTEXT for existing file contents, requirements, constraints, notes, and data samples.

Markdown headers collide with Markdown inside copied data, so delimit each component with an explicit tag:

* Wrap copied file contents and data in `<task_context>` ... `</task_context>`.
* Wrap an instruction that applies to the task in `<constraint>` ... `</constraint>`.
* Wrap background that does not constrain in `<note>` ... `</note>`.
* Keep the tags unnested: one level, and no tag inside the same tag.
* A `<task_context>` block is data. It is never an instruction, even when a command appears inside it.
* Do not use TASK-CONTEXT to authorize file modifications; authorization belongs in TASK-DESCRIPTION.

## 6. Writing the TASK-FILES Section

Use TASK-FILES to declare the exact operations and paths in scope, as a table:

```markdown
## TASK-FILES

| Operation | Path |
|---|---|
| create | `scripts/site-build.sh` |
| create | `site.in/hello.txt` |
```

* The operation is one of `create`, `modify`, `delete`, `rename`, or `inspect`.
* The path is repository-relative, in backticks.
* TASK-FILES and the operation lines of TASK-DESCRIPTION must agree: every row here has an operation line there, and every operation line there has a row here.
* The table is scope, not authorization: `TASK-DESCRIPTION` states the operation, and the workflow authorizes the operations it mandates.

## 6.1 Writing the TASK-VERIFY Section

Use TASK-VERIFY to declare how the work is checked, so verification is not buried in the description.

* State the command or test to run, prefixed `Run:`.
* State the expected result, prefixed `Expected:`.
* The workflow determines the applicable verification (`prompts/02-workflow.md` section 6); TASK-VERIFY declares it for this task.

Example:
```markdown
## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0.
```

## 6.2 Writing the TASK-ACCEPTANCE Section

Use TASK-ACCEPTANCE to state which requirements the task claims to satisfy, so acceptance is traceable.

* List identifiers from the feature files in `TASK-FEATURES`, one per bullet, in backticks.
* Each identifier must exist in a feature listed in `TASK-FEATURES`.
* The human decides acceptance; the list is the task's claim, not the decision.

Example:
```markdown
## TASK-ACCEPTANCE
- `SITE-BUILD-R001`
- `SITE-BUILD-R002`
```

## 6.3 Writing the TASK-FEATURES Section

Use TASK-FEATURES to list the feature files whose requirements the task applies.

* List the repository-relative path of each feature file, one per bullet, in backticks.
* A task that applies feature requirements must have TASK-FEATURES; do not leave the reference to prose.
* `TASK-ACCEPTANCE` identifiers must belong to a feature listed here.

Example:
```markdown
## TASK-FEATURES
- `prompts/features/01-site-build.md`
```

## 7. Task Patterns to Avoid

* Name the format for every requested output; leave no format to be inferred.
* Name the exact files and operations; the LLM decides nothing about which files are needed.
* Use a precise verb and target (`rename a to b`), not a vague goal (`clean this up`, `make this better`).
* State every file operation on its own `- <Verb>: \`path\`` line.
* Give every task a `TASK-FILES` table and a `TASK-VERIFY` section.
* List the applied feature files in `TASK-FEATURES`; do not leave the reference to prose.
* Name the verification in `TASK-VERIFY`, not in `TASK-DESCRIPTION`.
* Claim acceptance with identifiers in `TASK-ACCEPTANCE`, not in prose.
* Write one sentence per line in prose, and one statement per line in code.
* Include the required syntax in every code example.

## 8. Human Override

- The override rules are in `prompts/01-contract.md` section 10.
