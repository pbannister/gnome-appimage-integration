# Task: Implement the AppImage Integration feature

## TASK-DESCRIPTION
- Create: `sources/integration/appimage_integrator.h`
- Create: `sources/integration/appimage_integrator.cpp`
- Create: `sources/tools/appimage_integrate.cpp`
- Create: `scripts/program-install.sh`
- Create: `tests/integration_plan_test.cpp`
- Create: `tests/70-integration-sandbox.sh`
- Create: `tests/80-desktop-provenance.sh`
- Modify: `sources/CMakeLists.txt`
- Modify: `Makefile`
The integrator plans, installs, uninstalls, runs, and audits an AppImage integration, and implements the requirements in `prompts/features/09-appimage-integration.md`.
Every test runs under a temporary `$HOME`, and the tools install into `$HOME/.local/bin`, so a launcher action has a stable target.

## TASK-OUTPUT
Produce the complete new content of these files in this order, then the `VERIFICATION:` line:
1. `sources/integration/appimage_integrator.h`
2. `sources/integration/appimage_integrator.cpp`
3. `sources/tools/appimage_integrate.cpp`
4. `scripts/program-install.sh`
5. `tests/integration_plan_test.cpp`
6. `tests/70-integration-sandbox.sh`
7. `tests/80-desktop-provenance.sh`
8. `sources/CMakeLists.txt`
9. `Makefile`

## TASK-CONTEXT
<note>
Best practice for integration is documented in `documents/10-appimage-desktop-integration.md`. An install writes only under the user's home and requires no root; `make install` copies each program through a temporary name so replacing a running binary cannot fail.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/integration/appimage_integrator.h` |
| create | `sources/integration/appimage_integrator.cpp` |
| create | `sources/tools/appimage_integrate.cpp` |
| create | `scripts/program-install.sh` |
| create | `tests/integration_plan_test.cpp` |
| create | `tests/70-integration-sandbox.sh` |
| create | `tests/80-desktop-provenance.sh` |
| modify | `sources/CMakeLists.txt` |
| modify | `Makefile` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and `tests/70-integration-sandbox.sh` writes only under its temporary home.

## TASK-FEATURES
- `prompts/features/09-appimage-integration.md`

## TASK-ACCEPTANCE
- `APPIMAGE-INTEGRATION-R001`
- `APPIMAGE-INTEGRATION-R005`
- `APPIMAGE-INTEGRATION-R013`
- `APPIMAGE-INTEGRATION-R015`
- `APPIMAGE-INTEGRATION-R016`
- `APPIMAGE-INTEGRATION-R021`
- `APPIMAGE-INTEGRATION-R023`
- `APPIMAGE-INTEGRATION-R041`

OUTPUT: the nine complete files in the order listed, ending with the `VERIFICATION:` line.
