# Record: Adopt the shared rules

## Outcome

This project now pins the shared rules at `2026-10-22.13`, a version that exists
because of this adoption. `rules.lock` holds the version and a SHA-256 per
vendored file, `scripts/rules-check.sh` verifies the copy, and
`tests/08-rules-pin.sh` runs it in the suite.

The adoption was `breaking`: the contract gains a non-overridable safety
precedence level, modification becomes authorization-based, and nine capability
conventions apply.

The validator found only six things, because this project had tracked the rules
closely: 226 requirement identifiers across eight features were already in place,
and so was the task format. The damage was elsewhere.

## The failure worth recording

The vendor reported success. The validator reported six findings, all trivial.
Then nineteen of the thirty-nine test scripts failed at once.

`tests/lib/test_helpers.sh` is a vendored path. This project had extended it from
35 lines to 79, adding `build_program`, `build_synthetic_appimage` and
`DIRECTORY_BUILD`. The adoption's `--force` replaced the file with the shared
version, and every test that used those additions failed.

The library was wrong, not this project: a test helper is scaffolding, and
scaffolding is the thing a project legitimately grows. `2026-10-22.13` gives the
shared helper an extension point — it sources an optional, project-owned
`tests/lib/project_helpers.sh` — and this project's additions now live there, in
54 lines, untouched by any future upgrade. A superset shared helper was rejected,
because it would have put an AppImage builder into all nine other projects.

## Decisions

- **Two tests were renumbered.** `10-desktop-entry-reader.sh` collided with the
  vendored `10-prompt-validator.sh`, and `13-sha256.sh` with the new
  `13-corpus-coverage.sh`. They are now `11-` and `17-`. The areas keep their
  bands; one task that named the old file was updated. `records/` files that name
  them are records of what happened and were not rewritten.
- **The pin test is `08-rules-pin.sh`**, the only free slot in the `00`–`09` band.
- **`AGENTS.md` documents the test numbering**, because this project's bands are
  richer than the shared convention and a future reader needs to know which
  numbers are spoken for.
- **A glossary path was updated** to `01-universal-rules.md`, the name introduced
  in `2026-10-22.9`; this project's glossary still cited the old one.
- **`documents/07-appimage-format.md` left the feature graph.** It is a document,
  not a feature, and moved from `## Dependencies` to `## External Inputs`.
- **`PHASES.md` is declared** in `prompts/10-project.md`.
- **Three stale `common/` files were deleted** — the same boilerplate as
  everywhere else; their content was verified before deletion.

## Verification

- `rules-conform`: nothing to conform.
- `rules-check`: ok (v2026-10-22.13, 39 files, 0 deviations).
- `make test`: 39 test scripts, all passed — including the nineteen that failed
  before the helper was split.

## Behavioral evidence

**Not run.** Two of the six eval cases apply to this project —
`01-malformed-task-refusal` and `06-credential-boundary`: it carries
`scripts/site-state-fetch.sh` but not `scripts/status.sh`. Neither was run. The
portability gap is recorded in
`00-project-rules/documents/eval-portability.md`, and this project is the first
where two cases are runnable.

## What this adoption cost the library

One release, `.13`, for the helper extension point — the sixth shared-rules defect
found by an adoption, and the first found *after* the vendor and validator had
both reported success. That is worth keeping in mind for the remaining projects:
a clean vendor run is not evidence that the suite still works.

## Commits

- Adoption commit — the vendored rules, the split helper, and this record.
