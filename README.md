# Project Overview

- This repository is structured for collaborative development with a Large Language Model (LLM).
- This file `README.md` is located at the root of the project structure.

The LLM should begin by reading these files in this order (the numeric prefix marks the load order):

1. `prompts/01-contract.md`
2. `prompts/02-workflow.md`
3. `prompts/03-conventions.md`

- These define the interaction rules, workflow, and formatting conventions.
- The LLM must follow the workflow defined in `prompts/02-workflow.md` for every task.

Human contributors should begin by reading:

- `prompts/README.md`
- `documents/README.md`

Note there are rules meant only to constrain Aider behavior:

- `tools/aider-rules.md` (Aider users only)

All project features are defined in `prompts/features/` and implemented in `sources/`.

## Top-Level Map

- `README.md` is the project overview.
- `TODO.md` tracks pending and completed project tasks.
- `prompts/` contains LLM interaction rules, common requirements, feature requirements, task definitions, and episode work orders.
- `documents/` contains human-consumption documents: the interaction pattern, worked examples, and tool notes.
- `records/` contains version-controlled outcome, incident, and handoff records.
- `tools/` contains tool-specific rules.
- `tools/aider-rules.md` is used only with Aider.
- `sources/` contains implementations.
- `scripts/` contains project scripts.
- `tests/` contains tests and validation code.
- `dataflow.in/` contains input data.
- `dataflow.out/` contains generated data output (not version-controlled).
- `logs/` contains generated logs (not version-controlled).
- `site.in/` contains static-site input.
- `site.out/` contains generated static-site output (not version-controlled).
- `Makefile` drives the build (`make build`), the tests (`make test`), and cleanup (`make clean`).

## Worked Example

The repository includes one worked example that exercises the whole workflow:

- Feature: `prompts/features/01-site-build.md`
- Task: `prompts/tasks/01-site-build-implement.md`
- Script: `scripts/site-build.sh` generates `site.out/` from `site.in/`.
- Tests: `tests/00-skeleton.sh` and `tests/01-site-build.sh`
- Input: `site.in/index.txt` (status page) and `site.in/dashboard.txt`
- Template: `site.in/template.html` provides the HTML page structure.

- Run `make build` to generate the site and `make test` to run the tests.
- The bundled feature is a worked example, not a requirement.
- A new project keeps, trims, or repurposes it; real sites have used a static-site generator instead.

## Project Pages (publishing conventions)

- A project derived from this skeleton publishes the standard page set
  (status, dashboard, condensed todo/prompts/documents) per
  `prompts/features/02-project-pages.md` and
  `documents/06-project-pages.md`.
- `make site` builds the full set (`scripts/site-build.sh` +
  `scripts/site-condense.sh`).
- Publishing goes through the homelab project (homelab-publish): the project
  registers once via `pages_source` and does not push to the web server
  itself — `make deploy` is retired.

## Starting a New Project from this Skeleton

- Copy the repository, then decide what to keep, trim, or repurpose.
- Review `prompts/features/` and trim features the project does not need.
- Declare owner privacy boundaries (off-limits content) in the README or a dedicated document; the LLM treats them as authoritative scope exclusions.
- Adopt the test tiers in `prompts/02-workflow.md`; declare live-state tests and their prerequisites.
- Use records for outcomes, incidents, and handoffs; see `records/README.md`.
- Choose the concurrency model: git worktrees for parallel multi-thread development, checkpoint and handoff records for serial work against live systems.
- See `documents/04-lessons-from-homelab.md` for the lessons that shaped these rules.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `README.md`
- `TODO.md`
- `Makefile`
