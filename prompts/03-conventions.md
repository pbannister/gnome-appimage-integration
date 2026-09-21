# Conventions

These conventions define formatting, naming, and repository structure.

## 1. Formatting

- Use 4-space indents in code and Markdown when the format supports configurable indentation.
- Do not use tabs when the format supports spaces.
- Use one sentence per line in Markdown, so `git diff` is easier to read.
- Render a series of more than two one-sentence paragraphs as an unordered list when the sentences are parallel points that could be reordered.
- Keep prose for narrative progression, introductions, and sentences that lead into a following list.
- Break long quoted lists in shell scripts to one item per line, so `git diff` is easier to read.
- Use UPPERCASE names for shell variables that stay constant once defined.
- Use whole words in shell variable names; do not abbreviate.
- Give shell constants at least two words in semantic-sort order, broad first.
- Prefix shell variable names with the type word, like `file_input`.
- Use short, concise sentences in the style of Douglas Adams.
- Do not apply sentence-per-line rules to code blocks.
- Do not combine independent statements on one physical line.
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

## 6. Repository Hygiene

- Maintain `.gitignore` rules for generated output and logs.
- Preserve required empty directories with placeholder files.
- Generated files must be identified as generated.
- Do not edit generated files manually unless explicitly requested.
- Write generated output only to the designated output directory.
- Generated build trees are path-bound: clean them when the repository is reached through a different path.
- Do not mix source, prompt, and generated files.
- Commit messages use one line in imperative mood with a conventional prefix (`feat:`, `fix:`, `docs:`, `test:`, `chore:`, `refactor:`) and a short summary.
- A commit contains only the files of one completed task.
- Never commit generated output or logs.
- Live-state facts in hand-written documents carry a verification date: `verified 2026-08-22`.
- Prefer generated documents over hand-written ones for anything that reflects live state.

## 6.1 Generated Documentation

A project whose documents reflect live or derived state should keep a single source of truth and generate the documents from it:

- The model lives in `sources/`, for example `sources/<area>-model.yaml`.
- A generator script in `scripts/` produces the human documents from the model.
- The generator provides a validation mode that checks the model and exits nonzero on errors.
- Every generated document carries a provenance header: `Generated from <model> by <script> — do not edit by hand.`
- The documents index (`documents/README.md`) marks generated documents as generated.
- The generator's validation mode runs under `make test` when the project defines it.

## 6.2 Version Information

A program version is a build-time fact, not a source literal.

- Derive the version from git state: `date-branch-hash` or `date-branch-tag-hash`.
- `date` is the build date in `YYYY-MM-DD` form.
- `branch` is the current git branch.
- `tag` is appended only when all sources are committed and the current commit is tagged.
- `hash` is the short git commit hash; it disambiguates builds from different commits, directories, and developers.
- Suffix the hash with `-changes` when the working tree has uncommitted changes.
- A build-time script generates a header that contains the version components and a build counter.
- The build counter increments on each build; it is not checked in.
- Keep the version-reporting function in a separate compilation unit, so a version change recompiles one unit instead of the whole program.
- The generated header carries a provenance header and is not edited by hand.
- The generated header is written to the build output directory, not into `sources/`.
- Example: `2026-08-24-master-a1b2c3d` or `2026-08-24-master-v1.0.0-a1b2c3d` or `2026-08-24-master-a1b2c3d-changes`.
- The canonical generator name is `scripts/version-generate.sh`.
- The canonical version header is `version_info.h`.
- The canonical version functions are `version_string()` and `version_build_counter()`.

Decided 2026-08-24: the git hash disambiguates builds, so the build counter
policy is settled. Keep the local counter gitignored; never check it in.

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

## 8. Output

- Follow the exact output format specified by the task.
- Do not include assumptions or invented requirements.
- Do not modify unrelated files.
- Do not praise, approve, or compliment the human's statements.
- Do not open responses with agreement or affirmation filler.
- Compliments raise confidence, and an over-confident engineer makes errors.
- Treat praise as a bug: it is not informative and it distorts judgment.
