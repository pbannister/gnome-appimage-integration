# Task: Implement the AppImage Double-Click Handler feature

## TASK-DESCRIPTION
- Modify: `sources/tools/appimage_integrate.cpp`
- Create: `sources/tools/appimage_activator_ui.cpp`
- Modify: `sources/CMakeLists.txt`
- Modify: `scripts/program-install.sh`
- Modify: `Makefile`
Add the `handle`, `handler`, and activator commands, and implement the requirements in `prompts/features/10-appimage-handler.md`.
The activator is built only when the GTK4 development files are present, so the command-line tools still build without them.
Registration records the previous default for each AppImage MIME type before it changes anything, so `handler uninstall` can restore it.

## TASK-OUTPUT
Produce the complete new content of these files in this order, then the `VERIFICATION:` line:
1. `sources/tools/appimage_integrate.cpp`
2. `sources/tools/appimage_activator_ui.cpp`
3. `sources/CMakeLists.txt`
4. `scripts/program-install.sh`
5. `Makefile`

## TASK-CONTEXT
<note>
The user authorised registering this tool as the real `*.AppImage` default, which is a live-state change and not part of `make test`. AppImageLauncher owned the AppImage MIME definitions; a user-space copy at `$XDG_DATA_HOME/mime/packages/appimage.xml` preserves them if the package is removed.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| modify | `sources/tools/appimage_integrate.cpp` |
| create | `sources/tools/appimage_activator_ui.cpp` |
| modify | `sources/CMakeLists.txt` |
| modify | `scripts/program-install.sh` |
| modify | `Makefile` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and `tests/95-activator-ui.sh` reports ok or skips when GTK4 is absent.

## TASK-FEATURES
- `prompts/features/10-appimage-handler.md`

## TASK-ACCEPTANCE
- `APPIMAGE-HANDLER-R001`
- `APPIMAGE-HANDLER-R003`
- `APPIMAGE-HANDLER-R009`
- `APPIMAGE-HANDLER-R010`
- `APPIMAGE-HANDLER-R011`
- `APPIMAGE-HANDLER-R013`
- `APPIMAGE-HANDLER-R017`
- `APPIMAGE-HANDLER-R035`

OUTPUT: the five complete files in the order listed, ending with the `VERIFICATION:` line.
