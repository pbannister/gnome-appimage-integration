# Task: Implement the Desktop Entry Locator feature

## TASK-DESCRIPTION
* Create `sources/desktop/desktop_entry_locator.h`.
* Create `sources/desktop/desktop_entry_locator.cpp`.
* Create the portable test `tests/20-desktop-entry-locator.sh`.
* Create the live-state test `tests/50-desktop-search-path-live.sh`.
* Implement the requirements in `prompts/features/05-desktop-entry-locator.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/05-desktop-entry-locator.md`.
* Search-path facts and observed host values: `documents/09-desktop-file-search-paths.md`.
* The locator reads `XDG_DATA_HOME`, `XDG_DATA_DIRS`, `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`, and `HOME`.
* The live-state test declares its prerequisites in its header and reports PASS/WARN/FAIL.

## TASK-FILES
- `sources/desktop/desktop_entry_locator.h` — new
- `sources/desktop/desktop_entry_locator.cpp` — new
- `tests/20-desktop-entry-locator.sh` — new
- `tests/50-desktop-search-path-live.sh` — new
