# Task: Implement the Desktop Entry Locator feature

## TASK-DESCRIPTION
- Create: `sources/desktop/desktop_entry_locator.h`
- Create: `sources/desktop/desktop_entry_locator.cpp`
- Create: `tests/20-desktop-entry-locator.sh`
- Create: `tests/50-desktop-search-path-live.sh`
The locator builds the XDG application and autostart search paths, resolves a desktop file identifier, and implements the requirements in `prompts/features/05-desktop-entry-locator.md`.
The portable test builds its own XDG tree under a temporary directory; the live-state test compares the model with this host and declares its prerequisites.

## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/desktop/desktop_entry_locator.h`
2. `sources/desktop/desktop_entry_locator.cpp`
3. `tests/20-desktop-entry-locator.sh`
4. `tests/50-desktop-search-path-live.sh`

## TASK-CONTEXT
<note>
Search-path facts and the observed host values are in `documents/09-desktop-file-search-paths.md`. The locator reads `XDG_DATA_HOME`, `XDG_DATA_DIRS`, `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`, and `HOME`, and never writes.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/desktop/desktop_entry_locator.h` |
| create | `sources/desktop/desktop_entry_locator.cpp` |
| create | `tests/20-desktop-entry-locator.sh` |
| create | `tests/50-desktop-search-path-live.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, with `tests/50-desktop-search-path-live.sh` reporting PASS or WARN and never FAIL.

## TASK-FEATURES
- `prompts/features/05-desktop-entry-locator.md`

## TASK-ACCEPTANCE
- `DESKTOP-ENTRY-LOCATOR-R001`
- `DESKTOP-ENTRY-LOCATOR-R004`
- `DESKTOP-ENTRY-LOCATOR-R006`
- `DESKTOP-ENTRY-LOCATOR-R014`
- `DESKTOP-ENTRY-LOCATOR-R017`
- `DESKTOP-ENTRY-LOCATOR-R018`
- `DESKTOP-ENTRY-LOCATOR-R021`

OUTPUT: the four complete files in the order listed, ending with the `VERIFICATION:` line.
