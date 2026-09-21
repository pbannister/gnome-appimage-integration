# Task: Implement the Command-Line Tools feature

## TASK-DESCRIPTION
* Create `sources/tools/appimage_inspect.cpp`.
* Create `sources/tools/desktop_inspect.cpp`.
* Create `sources/CMakeLists.txt`.
* Create `scripts/program-build.sh`.
* Modify `Makefile` so that `build` runs `scripts/program-build.sh`.
* Create the integration test `tests/60-command-line-tools.sh`.
* Implement the requirements in `prompts/features/06-command-line-tools.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created and modified files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/06-command-line-tools.md`.
* Consumer features: `prompts/features/03-appimage-reader.md`, `prompts/features/04-desktop-entry-reader.md`, `prompts/features/05-desktop-entry-locator.md`.
* The build tree is written to `dataflow.out/build/`, which is generated output and not version-controlled.
* The Makefile keeps `site`, `clean`, `test`, `deploy`, and `install`; only `build` changes.

## TASK-FILES
- `sources/tools/appimage_inspect.cpp` — new
- `sources/tools/desktop_inspect.cpp` — new
- `sources/CMakeLists.txt` — new
- `scripts/program-build.sh` — new
- `tests/60-command-line-tools.sh` — new
- `Makefile` — existing
