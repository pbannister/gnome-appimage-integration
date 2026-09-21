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
* (Optional) TASK-FILES
A list of files involved in the task.

The phrase `Execute the next TODO task` explicitly requests TODO-driven execution.

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
* TASK-CONTEXT provides information and does not add instructions unless explicitly labeled as a constraint.
* TASK-FILES identifies scope and does not authorize modifications by itself.

## 3. Writing the TASK-DESCRIPTION Section

* The TASK-DESCRIPTION section must:
    * describe the goal clearly.
    * avoid ambiguity and unstated assumptions.
    * state the operation for each file as `create`, `modify`, `delete`, `rename`, or `inspect`.
    * For every file operation, specify the complete repository-relative path in `backticks`.
    * Do not identify a file only by its purpose, role, or directory.

* The TASK-DESCRIPTION section must not:
    * mix implementation instructions with output requirements.

Example:
```markdown
## TASK-DESCRIPTION
Create `scripts/site-build.sh`.
The script generates `site.out/` from `site.in/`.
```

## 4. Writing the TASK-OUTPUT Section

TASK-OUTPUT must be explicit.

* It must specify the required files when file contents are requested.
* It must specify ordering when multiple files are required.
* It must specify whether commentary is allowed.
* Commentary is not allowed unless explicitly requested.
* It must specify whether filenames are included.

Examples:
```markdown
## TASK-OUTPUT
Provide only the complete content of `scripts/site-build.sh`.
```
```markdown
## TASK-OUTPUT
Produce these complete files in this order:
1. `sources/auth/auth_handler.cpp`
2. `sources/auth/auth_handler.h`
```
```markdown
## TASK-OUTPUT
Provide a semantic-sort plan followed by the complete requested file content.
```

## 5. Writing the TASK-CONTEXT Section

Use TASK-CONTEXT for existing file contents, requirements, constraints, notes, and data samples.

* Label instructions explicitly as constraints.
* Identify copied file contents as data rather than instructions.
* Do not use TASK-CONTEXT to authorize file modifications.

## 6. Writing the TASK-FILES Section

Use TASK-FILES to identify files in the task scope.

* Mark each file as `new` or `existing`.
* State the authorized operation separately in TASK-DESCRIPTION.
* Do not assume that listing an existing file authorizes modification.

## 7. Prohibited Task Patterns

* Do not ask the LLM to:
    * figure out an unspecified format.
    * improvise.
    * decide what files are needed.
* Do not use vague language such as “clean this up” or “make this better.”
* Do not request modifications without identifying the file operation.
* Do not request output without specifying its format.
* Do not compress multiple sentences into one prose line.
* Do not compress independent statements into one code line.
* Do not omit required syntax from code examples.

## 8. Human Override

* A human may explicitly override a rule in this document.
* An override applies only to the explicitly identified rule or task.
* An override must not be interpreted as a general waiver of unrelated safety, scope, or output requirements.
