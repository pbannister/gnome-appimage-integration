# Record: graphical handler

## Outcome

The AppImage handler is now a real GTK4 dialog instead of a one-shot zenity prompt.
It shows what the AppImage is, remembers its window size, returns from Inspect to the
first window, and introspects existing launchers before integrating.

## The Requirements and How They Were Met

| Requirement | Implementation |
| ----------- | -------------- |
| The first window is too short, and a resized window should be remembered | the GTK handler saves `width` and `height` to `$XDG_DATA_HOME/gnome-appimage-integration/ui.json` on close and restores them with `set_default_size` on open; the default grew to 580x640 |
| Show version information | the version is extracted without running the AppImage: `X-AppImage-Version`, then AppStream `<release version=...>`, then a dotted version in the file name; the source is reported alongside the value |
| The first window should show Name, Comment, and GenericName | the main window shows the name in large type, then version and GenericName, then Comment, then a details grid with the file, type, size, payload, embedded entry, install id, and update information |
| Inspect should offer only Close and return to the first window | Inspect opens a separate window whose only button is Close; the main window stays open behind it |
| Integrate should introspect existing `.desktop` files | the conflict dialog lists each existing launcher's id, name, origin, version, target AppImage with a present/MISSING mark, icon, window class, and file path, then offers Replace existing, Add alongside, or Cancel |

## How the Handler Is Wired

- `appimage-integrate handle <AppImage>` prefers `python3 appimage_handler_ui.py --tool <tool> <AppImage>` when a display exists and `import gi; gi.require_version('Gtk','4.0')` succeeds.
- Otherwise it runs the previous zenity flow, and with no display it prints the equivalent commands.
- The Python script holds no integration logic: it calls `explain --json` to read, and `install`/`run` to act, so the C++ core remains the single source of truth.
- `sources/tools/appimage_handler_ui.py` is copied next to the tool by CMake (build tree) and by `scripts/program-install.sh` (`$HOME/.local/bin`).
- `explain --json` is new and carries the name, generic name, comment, version and its source, detection, sizes, compression, update information, identifiers, the embedded entry text, and the full conflict list.

## Version Extraction, Verified on Real AppImages

| AppImage | Version | Source |
| -------- | ------- | ------ |
| UltiMaker Cura | 5.10.1 | `X-AppImage-Version` |
| OpenShot | 3.3.0 | AppStream |
| FreeCAD 1.1.3 (in Downloads) | 1.1.3 | AppStream |

The AppImage format itself carries no version, so these three sources, in that order, are the complete answer.

## Verification

Verified 2026-09-22 with `make test`; all fifteen test scripts passed.

- `tests/95-handler-ui.sh` is new: it checks that the handler script is copied next to the tool, that it compiles, that the tool references it, and that the headless path prints instructions instead of hanging.
- `tests/90-integration-conflicts.sh` now also asserts the `explain --json` fields the dialog renders, including `version` and `version_source`.
- Real AppImages were read only; the handler window itself could not be exercised in this headless session, so the GTK drawing code is verified by compilation, import, and dispatch rather than by a screenshot.

## Commits

- `a6a1d52` feat: add a GTK handler with remembered geometry, version, and launcher introspection
