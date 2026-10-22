# Agent Entry Point

This file is the entry point for a coding agent working in this repository.
It is orientation only: it points at the authoritative files and defines no
rule of its own. Where this file and an authoritative file differ, the
authoritative file governs (`prompts/01-contract.md` section 2).

## Read First

- `prompts/01-contract.md` — authority, precedence, interaction, output, safety.
- `prompts/02-workflow.md` — the execution sequence; its section 1 lists the
  mandatory baseline, in order, and the task-relevant files to load after it.
- `prompts/03-conventions.md` — formatting, naming, and repository structure.

## Then

- The task: `prompts/how-to-write-tasks.md` defines the format a task must have.
- The feature requirements the task references: `prompts/features/`. The readers
  (AppImage, desktop entry, icon theme, MIME) come first; the integration and
  handler features build on them.
- The state: `TODO.md` (status, not authorization) and `PHASES.md` (phase plan).
- The record forms: `records/README.md`.

## Commands

- `make test` — the repository check suite; it writes a log under `logs/`.
- `make check` — validate generated output.
- `make site` — build `site.out/` from `site.in/`.
- `make release` — package what a release publishes.
- See the `Makefile` for the rest.

## Shared Rules

- `rules.lock` records which `00-project-rules` version these rules came from.
- The files it lists are vendored. Do not edit them here: a rule change belongs
  in `00-project-rules`, followed by a version bump and an upgrade that records
  the result. A project-specific rule goes in a local prompt file numbered after
  the shared ones; an override quotes the rule it replaces, so a later version
  can tell when the override has gone stale.
- `prompts/10-project.md` declares the root files this project keeps that the
  shared rules cannot know about.
- `make test` runs `scripts/rules-check.sh`, which fails on an edited vendored
  file, a local file that restates shared text, or a deviation whose quoted rule
  has moved.

## Test Numbering

The vendored tests claim `08`, `09`, `10` and `13`. This project's own tests keep
their area bands — `10`–`19` readers and parsers, `20`–`29` locators, `70`–`79`
integration sandboxes, `90`–`99` lifecycle — and two were renumbered on adoption
to free the vendored slots.

## Tool-Specific Files

- A tool-specific file (`.aider.conf.yml`, `CLAUDE.md`,
  `.github/copilot-instructions.md`, or similar) points here or holds
  mechanical configuration; it must not restate a rule.
- `prompts/` is the only place a rule is defined.
