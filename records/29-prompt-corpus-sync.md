# Record: the prompt corpus re-synced from the skeleton

## Outcome

The project's copy of the shared prompt corpus matches the project skeleton again, and the corpus now validates itself mechanically. All 256 requirements in `prompts/features/` carry a stable identifier, every task in `prompts/tasks/` declares its scope, verification, features, and accepted requirements, and the project runs its own sanitization gate before publishing.

## What Changed

- The skeleton's rule files were copied verbatim: `prompts/01-contract.md`, `02-workflow.md`, `03-conventions.md`, `prompts/README.md`, `prompts/common/02-universal-rules.md`, the three `how-to-write-*` guides, the new `how-to-write-research.md`, `prompts/episodes/01-episode-template.md`, the new `prompts/episodes/02-episode-plan.md`, `prompts/features/02-project-pages.md`, `records/README.md`, and `tests/README.md`.
- The append-only shared documents were restored to the skeleton's exact text, with this project's content below a `## How this project does it` heading: `prompts/common/03-glossary.md`, `prompts/episodes/00-episodes.md`, `documents/README.md`, and `documents/06-project-pages.md`.
- Every requirement in features `01` and `03`–`10` gained a `<FEATURE-NAME>-R<NNN>` identifier, 256 in total across the ten features, and each task claims the identifiers it satisfies in `TASK-ACCEPTANCE`.
- Each task `01`–`09` was rewritten in the new form: `- <Verb>: \`path\`` operation lines in `TASK-DESCRIPTION`, a matching `TASK-FILES` table, `TASK-VERIFY`, `TASK-FEATURES`, `TASK-ACCEPTANCE`, and a final `OUTPUT:` line.
- `tests/09-prompt-contract.sh` and its adversarial corpus `tests/10-prompt-validator.sh` were ported, so the registry, references, numbering, feature and task structure, traceability, and phase state are checked rather than trusted to prose.
- `scripts/leak-gate.sh` and `scripts/sensitive-patterns.sh` were ported, because the shared `PROJECT-PAGES-R017` requires the project to run its own gate over `site.out/`; `make site` now runs it, `make check` runs it alone, and `tests/04-leak-gate.sh` checks the gate and guards the single pattern source.
- `scripts/tests-run.sh` now runs every test and reports a `PASS`/`FAIL` line per test instead of stopping at the first failure, and `tests/lib/test_helpers.sh` gained `skip_test`/`skip_unless_tool`.
- `scripts/site-build.sh` now starts from an empty output tree and copies authored assets, matching the shared Site Build requirements.
- `PHASES.md` moved to `phase 5 — prompt corpus in step with the skeleton — complete`.

## Decisions

- Shared text is synchronized byte for byte; project content is appended below `## How this project does it`, never merged into the shared text, so the next re-sync stays a small diff.
- A requirement identifier is permanent: a changed requirement keeps its number, and a new requirement takes a new one.
- This project keeps its own sanitization gate rather than relying on the homelab's gate alone; both run, and the patterns have one source.
- Feature `01` here omits the skeleton's pipeline-artifact copy and content-versioned-asset requirements: this project publishes no pipeline artifact and serves no versioned asset.
- The skeleton's own phase-state fixture was tied to a project that has not started; the fixture was fixed there so derived projects do not have to fork the test.

## Verification

- `sh tests/09-prompt-contract.sh` reports ok, and `sh tests/10-prompt-validator.sh` reports ok with 27 malformed fixtures rejected — verified 2026-09-24.
- `make test` reports 28 tests passed with no failures and no skips (`logs/2026-09-24-21-25-14-test-run.log`) — verified 2026-09-24.
- `sh scripts/leak-gate.sh site.out` reports clean — verified 2026-09-24.
- From the skeleton, `sh scripts/skeleton-diff.sh ~/work/gnome-appimage-integration` reports 0 drift, 0 missing, 3 appended, and 2 adapted reference scripts — verified 2026-09-24.

## Commits

- `f0d822e` docs: sync the prompt corpus with the project skeleton
- `6e9c412` test: make the phase-state fixture independent of the current phase (project skeleton)
