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
- Load a language-specific flavor file only when the task targets that language.
- Load only the files explicitly referenced by the task and the files required by those references.
- Update `TODO.md` only when the task requests a TODO update or completes a TODO item (section 7).
- A TODO item alone is not authorization; the requested work comes from the task (see `prompts/01-contract.md` section 3).
- The phrase `Execute the next TODO task` selects the first unchecked item in `TODO.md`. A one-line item is not an executable task: draft a conforming task from it (`TASK-DESCRIPTION`, `TASK-OUTPUT`, `TASK-FILES`) and get it ratified before executing.

## 2. Interpret the Task

- Determine the requested operations, target files, constraints, and output format.
- Treat `TASK-DESCRIPTION` as the requested work.
- Treat `TASK-OUTPUT` as the response representation.
- Interpret `TASK-CONTEXT` by the delimiter rules in `prompts/how-to-write-tasks.md` section 5.
- Treat `TASK-FILES` as scope information only; authorization is in `TASK-DESCRIPTION`.
- For every file operation, resolve the exact path before implementation.
- For a `create` operation, verify that the target path is authorized by the task or by an applicable workflow step.
- For a `modify`, `delete`, or `rename` operation, verify that the referenced path exists or report that it is missing.
- If an exact path cannot be resolved unambiguously, stop and ask for clarification.
- Ask clarification questions before producing implementation output if any required detail is ambiguous or missing.
- Internally restate the task in one concise sentence.

## 2.1 Apply Feature and Task Scope

- Treat a referenced feature file as authoritative requirements for the current task.
- Apply a feature file only when the task references it or a referenced feature depends on it.
- Treat a task file as a detailed task description and apply its `TASK-DESCRIPTION`, `TASK-OUTPUT`, `TASK-CONTEXT`, and `TASK-FILES` sections according to `prompts/how-to-write-tasks.md`.
- When a task conflicts with a referenced feature, report the conflict, state which of the two you believe is stale and why, and ask which governs, unless the task explicitly overrides the feature requirement.

## 3. Plan the Work

- Plan before implementation, as a working aid: identify the applicable requirements, target files, required validation, and output order.
- Keep the plan out of the response unless the requested output format includes it.

## 4. Apply the Test Policy

- For executable source-code changes, create or update tests before implementation code.
- This policy authorizes creating or updating the tests the change requires in `tests/`; the task need not list those test files separately.
- For prompt, documentation, configuration, or build-script changes, add tests only when an applicable test mechanism exists or the task requests tests.
- Tests for source code belong in `tests/`.
- Tests for scripts belong in `tests/` and should validate the script behavior without placing generated output in source directories.
- Prompt validation belongs in `tests/` when a prompt validation mechanism exists or the task requests prompt validation.
- The prompt-contract test, `tests/09-prompt-contract.sh`, is that mechanism: it validates the registry, references, numbering, task and feature sections, and phase state.
- The project's existing test runner is sufficient; create one only when the task requests it.

### 4.1 Test Tiers

Classify every test into one tier:

- **Portable** — runs anywhere the repository is checked out; belongs in `tests/` and runs under `make test`.
- **Tool-gated** — needs a prerequisite tool or dependency; skips cleanly with a message when the prerequisite is absent.
- **Live-state** — verifies a documented model against live reality (a network, devices, a running system); requires declared access (hosts, keys, credentials); reports PASS/WARN/FAIL and exits nonzero only on failure.

Rules:

- A live-state test declares its prerequisites in its header comment, including where it must run and which keys or access it needs.
- A live-state test belongs in `tests/` only when the repository lives in the target environment; otherwise it is run by an explicit mechanism outside `make test`.
- A live-state test that could not check anything reports WARN or FAIL.
- Use PASS/WARN/FAIL classification: WARN for environment-dependent conditions, FAIL for broken invariants.
- A tool-gated test skips with the standard message `SKIP: missing tool: <tool>` (see `tests/lib/test_helpers.sh`).

### 4.2 Test Organization

- Number tests in reserved bands so lexical order is execution order; the bands are listed in `tests/README.md`.
- Share helpers in `tests/lib/`, sourced by a test and never executed by the runner.
- A test never touches the real system: sandbox `HOME` and XDG paths into a temporary directory, serve HTTP on `127.0.0.1`, and use no outside network.
- Keep tests that change live state outside `make test`.
- Keep spec-derived sample inputs in `dataflow.in/` and use them as fixtures.
- A behavioral fix to executable code ships a regression test that fails against the pre-fix implementation; a fix with no applicable test mechanism is exempt.
- Verification is independent of construction: do not use the builder's own helpers as the oracle.

## 5. Implement the Requested Scope

- The scope rules are in `prompts/common/02-universal-rules.md` (Scope Rules).
- Apply feature-specific requirements only when the feature is referenced by the task or by a directly referenced feature dependency.
- Create a file only when the task requires it; a directory being available is not a reason.
- Create one file per concept; create a companion file only when the task requests both.
- Preserve exact casing, separators, numbering, and extensions from the resolved path.

## 6. Verify the Work

Determine the verification applicable to the task (section 4.1), then perform it.

- Run `make test` when the repository test suite covers the changed artifacts.
- Run the relevant test directly when it is not part of `make test`, for example a live-state test.
- For a documentation-, prompt-, or configuration-only change, run the validation that applies to it when one exists.
- Claim a passing suite only after it ran successfully.
- If the applicable verification cannot be run, report that with the `VERIFICATION:` line instead of claiming completion.
- If it fails, correct the failure within task scope and run it again.
- If the failure cannot be corrected within task scope, stop, report the failure, and do not claim completion.
- Report verification with the `VERIFICATION:` line defined in `prompts/01-contract.md` section 5.

## 7. Update Task Status

- Update `TODO.md` only when the task explicitly requests a TODO update or completes a TODO item.
- Modify `TODO.md` only for the task's own status update.
- Mark a TODO item complete only after the requested verification succeeds.

## 7.1 Commit Completed Work

A project declares whether the LLM commits completed tasks. The setting lives in `README.md` or the applicable project rules; the default is enabled.

When automatic task commits are enabled:

- When the task changed files and verification succeeds, commit the completed work with git.
- Create one commit containing the task's files and any status update the task requires: `TODO.md`, or the status line of a referenced record. A status update rides with the change.
- Use the commit-message conventions in `prompts/03-conventions.md`.
- Commit the task's files and status updates, and nothing generated or unrelated.
- Test transcripts under `logs/` are verification artifacts; the commit excludes them unless the task explicitly requests one.
- Skip this step when the task changed no files.

When automatic task commits are disabled, or the task says the human owns commits:

- Leave the changes in the working tree.
- Report that verification succeeded and no commit was created.

An outcome record is written after the episode settles and the human reviews it (see `records/README.md`); commit it separately and cite the work commit. A record that cites its own commit hash is always that second commit. Write the real commit hash, never a placeholder.

## 7.2 Definition of Done

A task is complete only when every applicable item is satisfied:

- The requested scope is implemented with no out-of-scope changes.
- All verification applicable to the task was executed successfully.
- `make test` ran when it covers the changed artifacts; its omission is not a failure when it does not.
- A behavioral fix to executable code ships the regression test required by section 4.
- `TODO.md` is updated when the task requires a status update.
- Completed work is committed when automatic task commits are enabled and the task changed files (section 7.1).
- Output is produced in the requested format.

Verification produces evidence; acceptance remains the human's decision (see `prompts/common/03-glossary.md`).

## 8. Produce Output

- Produce output in the format specified by the task, and nothing beyond it.
- Keep working aids out of the response; include one only when the format requests it.
- Keep clarification, planning, implementation, and verification in separate responses.
- Produce multiple requested files complete, in the specified order.

## 9. Apply Corrections

- Apply the DELTA rules in `prompts/01-contract.md` section 6.

## 10. Stability

- Change this workflow only when explicitly instructed.
- Add a workflow step only when explicitly instructed.
- Perform every step this workflow requires.
