# Task: Implement the Desktop Entry Reader feature

## TASK-DESCRIPTION
- Create: `sources/desktop/desktop_entry_reader.h`
- Create: `sources/desktop/desktop_entry_reader.cpp`
- Create: `dataflow.in/desktop/spec-example.desktop`
- Create: `dataflow.in/desktop/localized.desktop`
- Create: `tests/10-desktop-entry-reader.sh`
The reader parses the `*.desktop` format into ordered groups, keys, and values, and implements the requirements in `prompts/features/04-desktop-entry-reader.md`.
The specification example and the localized sample are the fixtures the portable test reads.

## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/desktop/desktop_entry_reader.h`
2. `sources/desktop/desktop_entry_reader.cpp`
3. `dataflow.in/desktop/spec-example.desktop`
4. `dataflow.in/desktop/localized.desktop`
5. `tests/10-desktop-entry-reader.sh`

## TASK-CONTEXT
<note>
Format facts are in `documents/08-desktop-entry-format.md`; the language conventions are in `prompts/flavors/02-cpp-conventions.md` and the naming conventions in `prompts/flavors/01-semantic-sort-naming.md`. The parser is a library with no dependency on the rest of the project.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/desktop/desktop_entry_reader.h` |
| create | `sources/desktop/desktop_entry_reader.cpp` |
| create | `dataflow.in/desktop/spec-example.desktop` |
| create | `dataflow.in/desktop/localized.desktop` |
| create | `tests/10-desktop-entry-reader.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and `tests/10-desktop-entry-reader.sh` reports ok.

## TASK-FEATURES
- `prompts/features/04-desktop-entry-reader.md`

## TASK-ACCEPTANCE
- `DESKTOP-ENTRY-READER-R001`
- `DESKTOP-ENTRY-READER-R003`
- `DESKTOP-ENTRY-READER-R006`
- `DESKTOP-ENTRY-READER-R012`
- `DESKTOP-ENTRY-READER-R017`
- `DESKTOP-ENTRY-READER-R023`
- `DESKTOP-ENTRY-READER-R029`

OUTPUT: the five complete files in the order listed, ending with the `VERIFICATION:` line.
