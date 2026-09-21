# Task: Implement the Desktop Entry Reader feature

## TASK-DESCRIPTION
* Create `sources/desktop/desktop_entry_reader.h`.
* Create `sources/desktop/desktop_entry_reader.cpp`.
* Create the sample input `dataflow.in/desktop/spec-example.desktop` from the specification example.
* Create the sample input `dataflow.in/desktop/localized.desktop` with locale postfix keys.
* Create `tests/10-desktop-entry-reader.sh`.
* Implement the requirements in `prompts/features/04-desktop-entry-reader.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/04-desktop-entry-reader.md`.
* Format facts: `documents/08-desktop-entry-format.md`.
* Language conventions: `prompts/flavors/02-cpp-conventions.md`.
* Naming conventions: `prompts/flavors/01-semantic-sort-naming.md`.
* The parser must be a library with no dependency on the rest of the project.

## TASK-FILES
- `sources/desktop/desktop_entry_reader.h` — new
- `sources/desktop/desktop_entry_reader.cpp` — new
- `dataflow.in/desktop/spec-example.desktop` — new
- `dataflow.in/desktop/localized.desktop` — new
- `tests/10-desktop-entry-reader.sh` — new
