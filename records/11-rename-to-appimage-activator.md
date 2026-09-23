# Record: renaming the right-click item to AppImage Activator

## Outcome

The MIME handler's `Name=`, which is the label the right-click "Open With" menu shows, is now
**AppImage Activator**. "AppImage Handler" is another project's name. The entry, its record, its
icon, the window's program name, and `StartupWMClass` all moved with it, and `handler install`
migrates an existing install rather than leaving the old name behind.

## Why the Whole Name Moved, Not Only the Label

Renaming only `Name=` would have left `appimage-handler.desktop`, `appimage-handler.svg`, and
`appimage-handler.manifest` on disk, which is the same collision under a different surface: the
pre-rename entry still lists the AppImage MIME types, so it would still answer for them, and its
icon would still shadow the new name in the theme. The rename therefore covers every installed
artefact:

| Was | Is |
| --- | -- |
| `Name=AppImage Handler` | `Name=AppImage Activator` |
| `appimage-handler.desktop` | `appimage-activator.desktop` |
| `appimage-handler.manifest` | `appimage-activator.manifest` |
| `appimage-handler.svg` | `appimage-activator.svg` |
| `StartupWMClass=appimage-handler` | `StartupWMClass=appimage-activator` |
| `GLib.set_prgname("appimage-handler")` | `GLib.set_prgname("appimage-activator")` |
| `us.bannister.appimage-handler` | `us.bannister.appimage-activator` |
| `appimage_handler_ui.py` | `appimage_activator_ui.py` |

`tests/95-handler-ui.sh` became `tests/95-activator-ui.sh`.

## Migration

`handler install` now removes the pre-rename entry, icon, and record, and reports each one it
removed. Three details needed care:

- The AppImage MIME definition had already been rewritten from `application-x-executable` to
  `appimage-handler` by an earlier install, so the rewrite now replaces either the original
  generic icon or the pre-rename icon. Without that, the generic icon name in `appimage.xml`
  would have stayed at the old value and never named the new icon.
- The recorded previous defaults live in the record, so the new record is seeded from the
  pre-rename one when the new one does not exist yet. Otherwise the rename would have lost the
  `appimagelauncher.desktop` values that `handler uninstall` restores.
- A default that still names the pre-rename entry counts as ours when the previous default is
  resolved, so the marker alone is not mistaken for a real previous handler.

`handler status` now reports a leftover pre-rename entry and names the command that removes it.
`handler uninstall` reads either record and removes either name, so an install from before the
rename can still be reversed.

## Tests

`tests/95-activator-ui.sh` now sets up the pre-rename world in its isolated XDG home: the old
entry, the old icon, the old record, and a `mimeapps.list` that makes the old entry the default
for all three AppImage types. It then asserts that `handler install`:

- writes `Name=AppImage Activator`, `Icon=appimage-activator`, and
  `StartupWMClass=appimage-activator`;
- installs the activator icon and names it as the AppImage MIME generic icon, with no remaining
  `application-x-executable`;
- removes the pre-rename entry, icon, and record, and reports that it did;
- makes `appimage-activator.desktop` the default for `application/vnd.appimage`;
- carries `appimagelauncher.desktop` into the new record as the previous default.

It also asserts the window presents itself as AppImage Activator and is still multi-instance.
The tool's reference to the script is checked in the source, not the binary: the compiler merges
adjacent string literals, so the script name is not a literal substring of the executable.

`scripts/program-install.sh` removes the pre-rename script, the pre-rename icon next to it, and
the pre-rename icon in the theme, so nothing from the old name survives an install.

## State on This Host

Migrated and verified by hand:

- all three AppImage MIME types report `appimage-activator.desktop` via `xdg-mime query default`
  and `desktop-inspect --mime`;
- the record still holds `previous_default=…  appimagelauncher.desktop` for all three types;
- `appimage-activator.desktop` carries `Name=AppImage Activator`, `Icon=appimage-activator`, and
  `StartupWMClass=appimage-activator`; the old entry, icon, script, and record are gone;
- `desktop-inspect --icon appimage-activator` resolves to
  `~/.local/share/icons/hicolor/scalable/apps/appimage-activator.svg`;
- the MIME definition names `appimage-activator` as the generic icon.

## Also Removed

A tracked `sources/tools/__pycache__/appimage_handler_ui.cpython-312.pyc` had been committed by
an earlier round. It is gone, and `__pycache__/` and `*.pyc` are now ignored.

## Verification

Verified 2026-09-23 with `make test`; all fifteen test scripts passed, and the real handler state
above was read after `make install` and `appimage-integrate handler install`.

## Commits

- `e583f50` Rename the right-click item to AppImage Activator
