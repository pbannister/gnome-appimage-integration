# Task: Implement the AppImage Integration feature

## TASK-DESCRIPTION
* Create `sources/integration/appimage_integrator.h`.
* Create `sources/integration/appimage_integrator.cpp`.
* Create `sources/tools/appimage_integrate.cpp`.
* Create `scripts/program-install.sh` and wire `make install`.
* Create the sandbox test `tests/integration_plan_test.cpp` and its runner `tests/70-integration-sandbox.sh`.
* Create the provenance runner `tests/80-desktop-provenance.sh`.
* Implement the requirements in `prompts/features/09-appimage-integration.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created and modified files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/09-appimage-integration.md`.
* Best practice: `documents/10-appimage-desktop-integration.md`.
* Every test must run under a temporary `$HOME`; the real desktop must not be touched by a test.
* The installed tools live in `$HOME/.local/bin`, so launcher actions have a stable target.

## TASK-FILES
- `sources/integration/appimage_integrator.h` — new
- `sources/integration/appimage_integrator.cpp` — new
- `sources/tools/appimage_integrate.cpp` — new
- `scripts/program-install.sh` — new
- `tests/integration_plan_test.cpp` — new
- `tests/70-integration-sandbox.sh` — new
- `tests/80-desktop-provenance.sh` — new
- `sources/CMakeLists.txt` — existing
- `Makefile` — existing
