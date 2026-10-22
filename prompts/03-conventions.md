# Conventions

These conventions define formatting, naming, and repository structure.

## Applicability

- Every section of this file applies to every task.
- The capability conventions live in `prompts/capabilities/` and apply only when the project uses that capability; a project that does not use a capability ignores those rules, and a task that does not touch the capability does not load them.
    - `01-generated-documentation.md`
    - `02-version-information.md`
    - `03-generated-data-products.md`
    - `04-release-and-publishing.md`
    - `05-pinned-dependencies.md`
    - `06-execution-boundary.md`
    - `07-session-log.md`
    - `08-delegation-authority.md`
    - `09-failure-attribution.md`

## 1. Formatting

- Use 4-space indents in code and Markdown when the format supports configurable indentation.
- Use spaces where the format allows a choice.
- Use one sentence per line in Markdown, so `git diff` is easier to read.
- Render a series of more than two one-sentence paragraphs as an unordered list when the sentences are parallel points that could be reordered.
- Keep prose for narrative progression, introductions, and sentences that lead into a following list.
- Break long quoted lists in shell scripts to one item per line, so `git diff` is easier to read.
- Use UPPERCASE names for shell variables that stay constant once defined.
- Use whole words in shell variable names; do not abbreviate.
- Give shell constants at least two words in semantic-sort order, broad first.
- Prefix shell variable names with the type word, like `file_input`.
- Use short, concise sentences in the style of Douglas Adams.
- Apply sentence-per-line rules to prose only.
- Write one independent statement per physical line.
- Follow the target language's formatter and syntax rules.
- Follow the target language's brace and block syntax.
- Language and framework conventions override generic rules when required for correctness.

## 1.1 Comparison Conventions

- Compare constants first, on the left: write `5 == a`, never `a == 5`.
- An assignment where a comparison was intended becomes `5 = a`, which the compiler flags as an error.
- Write relational comparisons lesser-to-greater: write `5 < a`, never `a > 5`.
- Reading comparisons in one consistent order makes a reversed operator harder to miss.
- Combine both conventions when the constant is the lesser value: write `5 < a`, never `a > 5`.
- When the variable is the lesser value, keep it on the left: write `a < 5`, never `5 > a`.
- Use `<=` and `>=` only when the strict form is wrong; apply the same lesser-on-the-left order.

## 2. Directory Naming

The required directories are:
- `prompts/`
- `sources/`
- `scripts/`
- `tests/`          -- for unit, integration, script, and prompt-validation tests
- `dataflow.in/`    -- for input data
- `dataflow.out/`   -- for generated data output
- `logs/`           -- for generated logs
- `site.in/`        -- for static-site input
- `site.out/`       -- for generated static-site output
- `documents/`      -- for human-consumption documents
- `records/`        -- for version-controlled episode outcome records

Generated output directories are not version-controlled.

## 3. Filename Structure

- Feature numbers must be unique and must match the task, TODO item, or feature dependency.
- Use the established feature name.
- Do not invent a new feature name.
- Use whole words in directory and filenames.
- Do not use abbreviations in directory and filenames.
- Scripts use semantic-sort names such as `site-build.sh` and `site-sync.sh`.
- Script names order components from broad meaning to narrow meaning: `<domain>-<role>.sh`.
- Source modules use names such as `aspect_facet_category.ext`.

## 3.1 Filename Authority

- Use an existing filename when the task identifies an existing file.
- Before creating a file, reuse an established filename pattern.
- A filename is valid only if it is:
    - explicitly named by the task.
    - already present in the repository.
    - required by an established language, framework, or tool convention.
    - required by a referenced feature specification.
    - free of spaces and non-ASCII characters.
- Do not invent filenames from an informal description.
- Do not create synonymous, abbreviated, pluralized, or alternative filenames for an existing concept.
- If more than one filename is plausible, ask for clarification.
- If the required filename cannot be determined from the task, repository, or conventions, ask instead of guessing.
- Do not rename an existing file unless the task explicitly requests it.

## 4. Identifier Naming

- Apply the rules in `prompts/flavors/01-semantic-sort-naming.md`.
- Language-specific naming rules live in `prompts/flavors/`.

## 5. Documentation

- Document public APIs, non-obvious behavior, invariants, side effects, and externally visible formats.
- Do not add comments that merely restate the code.
- Include comments only when requested or when they explain non-obvious behavior.
- Customize a shared convention document by appending a project section (for example `## How this project does it`); do not fork the shared text, so a later re-sync stays a small diff (see `scripts/skeleton-diff.sh`).

## 6. Repository Hygiene

- Maintain `.gitignore` rules for generated output and logs.
- Preserve required empty directories with placeholder files.
- Identify every generated file as generated.
- Edit a generated file only when the task explicitly requests it.
- Write generated output only to its designated output directory.
- Generated build trees are path-bound: clean them when the repository is reached through a different path.
- Keep source, prompt, and generated files in their own directories.
- Commit messages use one line in imperative mood with a conventional prefix (`feat:`, `fix:`, `docs:`, `test:`, `chore:`, `refactor:`) and a short summary.
- A commit contains one task's files and that task's status updates; an outcome record is a separate commit (see `prompts/02-workflow.md` §7.1).
- Commit source, prompts, records, and configuration only.
- Live-state facts in hand-written documents carry a verification date: `verified 2026-08-22`.
- Prefer generated documents over hand-written ones for anything that reflects live state.
- Run the publish sanitization gate (`scripts/leak-gate.sh`) over generated output before publishing.
- Keep the sanitization patterns in one source (`scripts/sensitive-patterns.sh`) and never inline them in another script or test.
- A served static asset is requested under a URL that changes when its bytes change: derive one content token from the interdependent asset set and append it to every URL, including transitive module imports; fail the build when the stamp does not land.
- Committed tool configuration uses workspace-relative paths (for example `${workspaceFolder}`), never an absolute home path; an absolute path is both a leak and a portability bug.

## 6.1 Third-Party Checkouts

A cloned third-party repository is host material, not project material.
It is an input a task may read, never a directory the project commits.

- Clone a third-party repository under the host checkout root `$HOME/sources/`, never inside the project working tree.
- The host checkout root is a host path and is not the repository's `sources/` directory; the two share a name and nothing else.
- Write it environment-relative as `$HOME/sources/`; a literal absolute home path is refused by the committed-path rule in `prompts/01-contract.md` section 7.
- A project whose host checkout root differs declares it in its project section.
- Pin a checkout the project depends on, per `prompts/capabilities/05-pinned-dependencies.md`.
- A checkout found inside the working tree is a defect: move it to the host checkout root and remove the in-tree copy.

## 7. File Operations

Every task must identify each file operation as one of:

- `create`
- `modify`
- `delete`
- `rename`
- `inspect`

Also:

- A file listed as existing is not automatically authorized for modification.
- For every `create`, `rename`, or `delete` operation, the task must identify the exact source and target filename.
- A directory name alone does not authorize creating a file with an invented name.

## 7.1 Unattended Scripts

A script must complete without a human at the terminal.

- A script must not block on standard input. A prompt that waits for an answer hangs an unattended run instead of failing it.
- When a script needs a choice, take it from an environment variable or an explicit command-line flag.
- The unattended default must fail closed: refuse, exit nonzero, and state the reason.
- A prompt is permitted only in a script the caller has explicitly marked interactive.
- `scripts/install.sh` is the worked example: it refuses when it cannot verify, names the environment variable that overrides the refusal, and never prompts.

## 8. Output

- The output rules are in `prompts/01-contract.md` section 5; the scope rules are in `prompts/common/01-universal-rules.md`.
- Praise and affirmation filler are output defects: they raise confidence without adding information.
