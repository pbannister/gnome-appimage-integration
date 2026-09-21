# GNOME AppImage Integration

This project reads AppImage files and `*.desktop` files so that an AppImage can be
integrated into the GNOME application menu.

The project is derived from `00-project-skeleton`.
The skeleton's interaction rules, workflow, and conventions apply unchanged.

## For the LLM

The LLM should begin by reading these files in this order (the numeric prefix marks the load order):

1. `prompts/01-contract.md`
2. `prompts/02-workflow.md`
3. `prompts/03-conventions.md`

- These define the interaction rules, workflow, and formatting conventions.
- The LLM must follow the workflow defined in `prompts/02-workflow.md` for every task.
- This project targets C++, so `prompts/flavors/02-cpp-conventions.md` applies to source work.

## For Human Contributors

Human contributors should begin by reading:

- `prompts/README.md`
- `documents/README.md`

## Project Scope

The project reads two file formats and locates one of them on disk.

1. Read an AppImage container: image type, ELF facts, payload offset and size, embedded update information and signature, and the SquashFS payload.
2. Read a desktop entry file: groups, keys, values, localized values, lists, escapes, actions, and `Exec` field codes.
3. Locate desktop entry files using the GNOME (GIO) search path built from `$XDG_DATA_HOME` and `$XDG_DATA_DIRS`.

The project does not execute, mount, or modify an AppImage, and does not write desktop entries.

## Research Findings

The format research that the readers implement is recorded in `documents/`.

- `documents/07-appimage-format.md` — the AppImage specification, the payload offset algorithm, and the SquashFS superblock.
- `documents/08-desktop-entry-format.md` — the Desktop Entry Specification version 1.5.
- `documents/09-desktop-file-search-paths.md` — where GNOME looks for `*.desktop` files, with the observed host values.

## Top-Level Map

- `README.md` is the project overview.
- `TODO.md` tracks pending and completed project tasks.
- `PHASES.md` records the current phase and its milestones.
- `prompts/` contains LLM interaction rules, common requirements, feature requirements, task definitions, and episode work orders.
- `documents/` contains human-consumption documents: the interaction pattern, worked examples, tool notes, and the format research.
- `records/` contains version-controlled outcome, incident, and handoff records.
- `tools/` contains tool-specific rules.
- `tools/aider-rules.md` is used only with Aider.
- `sources/` contains the C++ readers and command-line tools.
- `scripts/` contains project scripts.
- `tests/` contains tests and validation code.
- `dataflow.in/` contains input data, including sample desktop entry files.
- `dataflow.out/` contains generated data output and the build tree (not version-controlled).
- `logs/` contains generated logs (not version-controlled).
- `site.in/` contains static-site input.
- `site.out/` contains generated static-site output (not version-controlled).
- `Makefile` drives the build (`make build`), the tests (`make test`), the pages (`make site`), and cleanup (`make clean`).

## Building and Testing

- `make build` configures and compiles the readers and command-line tools into `dataflow.out/build/`.
- `make test` runs every test in `tests/` and writes a timestamped log to `logs/`.
- `make site` builds the published page set.
- `make clean` removes generated output.

The C++ build uses CMake with the highest warning level and treats warnings as errors.

## Source Layout

- `sources/appimage/` contains the AppImage container reader and the SquashFS payload reader.
- `sources/desktop/` contains the desktop entry reader and the desktop entry locator.
- `sources/tools/` contains the command-line front ends.
- `sources/CMakeLists.txt` defines the targets; the build tree is written to `dataflow.out/build/`.

## Project Pages (publishing conventions)

- A project derived from the skeleton publishes the standard page set
  (status, dashboard, condensed todo/prompts/documents) per
  `prompts/features/02-project-pages.md` and
  `documents/06-project-pages.md`.
- `make site` builds the full set (`scripts/site-build.sh` +
  `scripts/site-condense.sh`).
- Publishing goes through the homelab project (homelab-publish): the project
  registers once via `pages_source` and does not push to the web server
  itself — `make deploy` is retired.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `README.md`
- `TODO.md`
- `Makefile`
