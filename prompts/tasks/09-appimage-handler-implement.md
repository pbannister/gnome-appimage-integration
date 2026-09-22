# Task: Implement the AppImage Double-Click Handler feature

## TASK-DESCRIPTION
* Add the `handle` and `handler` commands to `sources/tools/appimage_integrate.cpp`.
* Implement the requirements in `prompts/features/10-appimage-handler.md`.
* Install the tools with `make install`.
* Register the handler with `appimage-integrate handler install`.
* Verify the default with `xdg-mime query default` for each AppImage MIME type.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the registration result, the recorded previous defaults, and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/10-appimage-handler.md`.
* The user authorised registering this tool as the real `*.AppImage` default.
* Registration records the previous default so `handler uninstall` can restore it.
* AppImageLauncher owned the AppImage MIME definitions; a user-space copy at `$XDG_DATA_HOME/mime/packages/appimage.xml` preserves them if the package is removed.

## TASK-FILES
- `sources/tools/appimage_integrate.cpp` — existing
- `scripts/program-install.sh` — existing
- `Makefile` — existing
