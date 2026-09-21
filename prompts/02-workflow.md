# LLM Workflow

This workflow defines the execution sequence for tasks in this project.

## 1. Load Project Rules

Always load these files in this order before executing a task:

- `prompts/01-contract.md`
- `prompts/02-workflow.md`
- `prompts/03-conventions.md`
- `prompts/common/00-overview.md`
- `prompts/common/01-requirements.md`
- `prompts/common/02-universal-rules.md`
- `prompts/common/03-glossary.md`
- `prompts/flavors/01-semantic-sort-naming.md`

Also:

- Load only the feature files explicitly referenced by the task or by a directly referenced feature dependency.
- Do not load unrelated feature files.
- Load a language-specific flavor file only when the task targets that language.
- Load only the files explicitly referenced by the task and the files required by those references.
- Use `TODO.md` only for status updates when the task explicitly requests them.
- Do not infer the requested work from the first unchecked TODO item.
- The phrase `Execute the next TODO task` explicitly requests TODO-driven execution.

## 2. Interpret the Task

- Determine the requested operations, target files, constraints, and output format.
- Treat `TASK-DESCRIPTION` as the requested work.
- Treat `TASK-OUTPUT` as the response representation.
- Treat `TASK-CONTEXT` as information that does not add instructions unless explicitly labeled as a constraint.
- Treat `TASK-FILES` as scope information that does not authorize modifications by itself.
- For every file operation, resolve the exact path before implementation.
- For a `create` operation, verify that the target path is authorized by the task or by an applicable project rule.
- For a `modify`, `delete`, or `rename` operation, verify that the referenced path exists or report that it is missing.
- If an exact path cannot be resolved unambiguously, stop and ask for clarification.
- Ask clarification questions before producing implementation output if any required detail is ambiguous or missing.
- Internally restate the task in one concise sentence.

## 2.1 Apply Feature and Task Scope

- Treat a referenced feature file as authoritative requirements for the current task.
- Do not apply unreferenced feature files.
- Treat a task file as a detailed task description and apply its `TASK-DESCRIPTION`, `TASK-OUTPUT`, `TASK-CONTEXT`, and `TASK-FILES` sections according to `prompts/how-to-write-tasks.md`.
- When a task conflicts with a referenced feature, ask for clarification unless the task explicitly overrides the feature requirement.

## 3. Plan the Work

- Create a concise internal plan before implementation.
- The internal plan must identify the applicable requirements, target files, required validation, and output order.
- The plan must not appear in the response unless the requested output format includes it.

## 4. Apply the Test Policy

- For executable source-code changes, create or update tests before implementation code.
- For prompt, documentation, configuration, or build-script changes, add tests only when an applicable test mechanism exists or the task requests tests.
- Tests for source code belong in `tests/`.
- Tests for scripts belong in `tests/` and should validate the script behavior without placing generated output in source directories.
- Prompt validation belongs in `tests/` when a prompt validation mechanism exists or the task requests prompt validation.
- Do not create a test runner solely to satisfy this policy unless the task requests one.

### 4.1 Test Tiers

Classify every test into one tier:

- **Portable** — runs anywhere the repository is checked out; belongs in `tests/` and runs under `make test`.
- **Tool-gated** — needs a prerequisite tool or dependency; skips cleanly with a message when the prerequisite is absent.
- **Live-state** — verifies a documented model against live reality (a network, devices, a running system); requires declared access (hosts, keys, credentials); reports PASS/WARN/FAIL and exits nonzero only on failure.

Rules:

- A live-state test declares its prerequisites in its header comment, including where it must run and which keys or access it needs.
- A live-state test belongs in `tests/` only when the repository lives in the target environment; otherwise it is run by an explicit mechanism outside `make test`.
- A live-state test must not pass silently when it could not check anything; it reports WARN or FAIL.
- Use PASS/WARN/FAIL classification: WARN for environment-dependent conditions, FAIL for broken invariants.

## 5. Implement the Requested Scope

- Modify only files within the declared task scope.
- Do not perform opportunistic refactoring.
- Do not reformat unrelated lines.
- Do not update dependencies, generated files, or documentation unless requested or required for correctness.
- Apply feature-specific requirements only when the feature is referenced by the task or by a directly referenced feature dependency.
- Do not create a file merely because its directory is available.
- Do not create a second file to replace or supplement an existing file unless the task explicitly requests both files.
- Preserve exact casing, separators, numbering, and extensions from the resolved path.

## 6. Verify the Work

- When execution tools are available, run `make test` from the repository root.
- Never claim that tests passed unless `make test` was actually executed successfully.
- If `make test` cannot be run, report that verification was not performed when verification results are requested.
- If `make test` fails, correct the failure within task scope and run `make test` again.
- If the failure cannot be corrected within task scope, stop, report the failure, and do not claim completion.

## 7. Update Task Status

- Update `TODO.md` only when the task explicitly requests a TODO update or completes a TODO item.
- Do not modify `TODO.md` as a side effect of unrelated work.
- Mark a TODO item complete only after the requested verification succeeds.

## 7.1 Commit Completed Work

- When the task changed files and verification succeeds, commit the completed work with git.
- Create one commit containing only the task's files and any TODO update made for the task.
- When the task changes a status, update the referenced record in the same commit.
- Use the commit-message conventions in `prompts/03-conventions.md`.
- Do not commit generated output, logs, or unrelated files.
- Skip this step when the task changed no files.

## 7.2 Definition of Done

A task is complete only when every applicable item is satisfied:

- The requested scope is implemented with no out-of-scope changes.
- `make test` executed successfully.
- `TODO.md` is updated when the task requires a status update.
- Completed work is committed when the task changed files.
- Output is produced in the requested format.

## 8. Produce Output

- Produce output only in the format specified by the task.
- Do not include the internal restatement or plan unless requested.
- Do not mix clarification, planning, implementation, and verification output.
- When multiple files are requested, produce complete files in the specified order.

## 9. Apply Corrections

- Treat a correction as a DELTA when the human uses the DELTA protocol.
- Apply only the requested DELTA changes.
- Ask for clarification if the change cannot be isolated to the named portions.

## 10. Stability

- Do not change this workflow unless explicitly instructed.
- Do not introduce new workflow steps.
- Do not skip required workflow steps.
