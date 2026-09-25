# Conventions

These conventions define formatting, naming, and repository structure.

## Applicability

- Sections 1–6 are core conventions: they apply to every task.
- Sections 6.1–6.5 are capability conventions: they apply only when the project uses that capability. A project that does not use a capability ignores those rules, and a task that does not touch the capability does not apply them.
    - 6.1 Generated Documentation
    - 6.2 Version Information
    - 6.3 Generated Data Products
    - 6.4 Release and Publishing
    - 6.5 Pinned Dependencies

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
- Keep `PHASES.md` current: it names the project's phases and the current phase, one `Current: phase N — [description —] state` line; change the current phase only when committing the project.

## 6.1 Generated Documentation (capability)

A project whose documents reflect live or derived state should keep a single source of truth and generate the documents from it:

- The model lives in `sources/`, for example `sources/<area>-model.yaml`.
- A generator script in `scripts/` produces the human documents from the model.
- The generator provides a validation mode that checks the model and exits nonzero on errors.
- Every generated document carries a provenance header: `Generated from <model> by <script> — do not edit by hand.`
- The documents index (`documents/README.md`) marks generated documents as generated.
- The generator's validation mode runs under `make test` when the project defines it.

## 6.2 Version Information (capability)

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

## 6.3 Generated Data Products (capability)

A project that generates data (not only documents) follows the same
single-source rule, with a machine-readable contract:

- A pipeline stage has one number shared by its feature, script, test, and work
  product: feature `03`, script `scripts/03-<stage>.sh`, work product
  `dataflow.out/03_<name>.json`.
- Each stage reuses an existing work product; it rewrites the file only when
  explicitly asked (`--refresh`), and fills gaps left by partial failure with
  `--retry-failed` where that is normal.
- A reuse check compares the work product's content and timestamp; presence
  alone is not enough.
- The make rule names the file it produces, so a stage whose output exists does
  not run. Never declare a placeholder variable or a rule for a file no program
  produces: an unreachable prerequisite leaves the target permanently stale and
  re-runs it on every invocation.
- A step that annotates an existing work product in place instead of producing
  its own file is a phony target, and says so.
- A default `all` target builds the pipeline and the site, and is a cheap no-op
  when everything is current.
- Raw inputs are cached separately from work products (`dataflow.out/raw/`),
  with provenance: `SHA256SUMS` and `FETCHED-AT.txt`, fetched only under
  `--refresh`.
- Every generated data product is described by a `manifest.json` beside it,
  carrying its parameters, its entries, and provenance: source dataset,
  generating script, timestamp, and git commit.
- A `check` mode validates existing output against the manifest without
  rebuilding; `--check` is an accepted alias.
- `make clean` removes generated output but preserves raw caches. When a
  rebuild is expensive or can only be repeated against a live system, `clean`
  prints the command instead of running it.
- Verification is independent of construction: probe the artifact (re-read the
  file, ray-cast, signed volume, digest) rather than calling the builder's own
  helpers as the oracle.
- An exchange-format export (STEP, OBJ, glTF) declares its units, and a test
  parses the file back.

## 6.4 Release and Publishing (capability)

A release publishes an artifact; it must be the artifact of the commit it
names. The build-time version from 6.2 is what makes that checkable.

- A release refuses a version that carries `-changes` (the tree was dirty at
  build time) and a version that does not name the current short commit hash.
  `scripts/release-gate.sh` performs both checks.
- Build from the commit being released inside the publish path, so a stale
  build tree cannot be published.
- Pack deterministically: sorted names, no owner, the commit's timestamp, and
  no gzip timestamp, so packing the same build twice yields one digest.
- Name release assets without a version, so
  `releases/latest/download/<asset>` works without an API call; a version can
  still be pinned by adding the tag to the path.
- Publish a checksum (`SHA256SUMS`), a `VERSION` file, and generated release
  notes that carry the install and verify commands.
- An installer is POSIX shell (it may be piped), verifies the download against
  the checksum, and refuses when verification is impossible; a named override
  is the only way to skip the check.
- Installing the tools is safe; claiming a system default (a MIME handler, a
  desktop entry) is a deliberate step the installer prints, not performs.
- Installing over a running program writes a temporary name and renames it
  over the destination.
- A publish is re-runnable: reuse the tag the commit already carries, refuse a
  tag on a different commit, skip a redundant push, and re-upload assets.

## 6.5 Pinned Dependencies (capability)

- Pin a vendored or forked dependency to a release tag, never a branch: a
  branch pin cannot be rebased against a fixed base.
- Record the pin in the source README or the document that owns it.
- A test asserts the pin in both places: the script's constant and the
  recorded value match, and the pin has the expected shape.
- When the heavy dependency cannot run in the test environment, the test
  asserts the script interface and the pin instead, and cross-checks an
  implementation against the system tool where one exists.

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

- The output rules are in `prompts/01-contract.md` section 5; the scope rules are in `prompts/common/02-universal-rules.md`.
- Praise and affirmation filler are output defects: they raise confidence without adding information.
