# Task: Implement the Command-Line Tools feature

## TASK-DESCRIPTION
- Create: `sources/tools/appimage_inspect.cpp`
- Create: `sources/tools/desktop_inspect.cpp`
- Create: `sources/tools/desktop_entry_output.h`
- Create: `sources/version/version.h`
- Create: `sources/version/version_info.cpp`
- Create: `sources/CMakeLists.txt`
- Create: `scripts/program-build.sh`
- Create: `scripts/version-generate.sh`
- Create: `tests/60-command-line-tools.sh`
- Modify: `Makefile`
The two inspection tools print the AppImage facts and the desktop entry facts, and implement the requirements in `prompts/features/06-command-line-tools.md`.
The build tree is written to `dataflow.out/build/`, which is generated output and not version-controlled.
The Makefile keeps its `site`, `clean`, `test`, `deploy`, and `install` targets; only `build` changes, so it runs `scripts/program-build.sh`.

## TASK-OUTPUT
Produce the complete new content of these files in this order, then the `VERIFICATION:` line:
1. `sources/tools/appimage_inspect.cpp`
2. `sources/tools/desktop_inspect.cpp`
3. `sources/tools/desktop_entry_output.h`
4. `sources/version/version.h`
5. `sources/version/version_info.cpp`
6. `sources/CMakeLists.txt`
7. `scripts/program-build.sh`
8. `scripts/version-generate.sh`
9. `tests/60-command-line-tools.sh`
10. `Makefile`

## TASK-CONTEXT
<note>
The command-line tools consume `prompts/features/03-appimage-reader.md`, `prompts/features/04-desktop-entry-reader.md`, and `prompts/features/05-desktop-entry-locator.md`. The tools never access the network and never execute an AppImage payload.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/tools/appimage_inspect.cpp` |
| create | `sources/tools/desktop_inspect.cpp` |
| create | `sources/tools/desktop_entry_output.h` |
| create | `sources/version/version.h` |
| create | `sources/version/version_info.cpp` |
| create | `sources/CMakeLists.txt` |
| create | `scripts/program-build.sh` |
| create | `scripts/version-generate.sh` |
| create | `tests/60-command-line-tools.sh` |
| modify | `Makefile` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and both `appimage-inspect --help` and `desktop-inspect --help` exit 0.

## TASK-FEATURES
- `prompts/features/06-command-line-tools.md`

## TASK-ACCEPTANCE
- `COMMAND-LINE-TOOLS-R001`
- `COMMAND-LINE-TOOLS-R002`
- `COMMAND-LINE-TOOLS-R005`
- `COMMAND-LINE-TOOLS-R011`
- `COMMAND-LINE-TOOLS-R016`
- `COMMAND-LINE-TOOLS-R019`
- `COMMAND-LINE-TOOLS-R024`

OUTPUT: the ten complete files in the order listed, ending with the `VERIFICATION:` line.
