# Record: integration failures and window geometry

## Outcome

The "Replace existing" failure was reproduced, explained, and made visible.
The window geometry is now remembered for every handler window, with the position kept only
where the platform permits it.

## Why "Replace existing" Appeared to Fail

The real desktop state showed that the FreeCAD 1.1.3 integration had in fact succeeded:
manifest `11aaf8de04157bd4`, launcher `org.freecad.FreeCAD.desktop`, target
`~/Applications/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage`, and the AppImageLauncher launcher moved
into the backup directory. The failing attempts were retries, and three defects made them confusing.

| Defect | Effect | Fix |
| ------ | ------ | --- |
| The dialog showed `stdout or stderr`, and `install` prints the plan to stdout before it can fail | the failure dialog showed the plan, which looks like success, and hid the error on stderr | the dialog now shows both streams and labels the error output; a failed install is titled "Could not integrate" |
| A retry used the original `Downloads` path, but a successful run had already moved the file | the plan failed with "cannot open file" and looked like a fresh bug | the handler now detects that the file is gone and says so, pointing at the application menu; `install` also treats "source gone, target present" as already placed instead of failing |
| Every replace renamed the displaced launcher onto the same backup name | an earlier backup could be overwritten | backup names are now made unique with a numeric suffix |

## The `.desktop` Installed as an Icon

The real manifest contained:

```
icon=…/hicolor/256x256/apps/appimage_11aaf8de_org.freecad.FreeCAD.desktop
```

The root-icon matcher accepted any payload root file whose name began with the icon base, and
FreeCAD's payload root contains `org.freecad.FreeCAD.desktop` next to `org.freecad.FreeCAD.svg`.
The desktop entry was therefore copied as a 256x256 icon.

- Fixed: root and themed icons are now accepted only with a `.png`, `.svg`, `.svgz`, or `.xpm` extension.
- Cleaned: the two stray files written by the earlier build were removed from `~/.local/share/icons/hicolor/256x256/apps/`.
- Regression: `tests/90-integration-conflicts.sh` now fails if any manifest `icon=` line ends in `.desktop`.

## Window Geometry

- Every window has a key: `main`, `inspect`, `conflict`, and `result`.
- Size is saved whenever `default-width` or `default-height` changes, and again on close, into `$XDG_DATA_HOME/gnome-appimage-integration/ui.json`.
- Size is restored with `set_default_size`, clamped to the monitor work area so a window is never larger than the screen.
- The position is stored as `x` and `y` when it can be read, and a stored position is clamped so the window stays entirely on the screen before it is applied.

## The Wayland Limitation

Position cannot be restored on a Wayland session.
Wayland deliberately does not let a client choose or read its own toplevel position; the compositor owns placement, and GTK4 consequently has no move or get-position API.
The user's session here is `XDG_SESSION_TYPE=wayland`.

The handler therefore:

- always remembers and restores the size;
- reads and restores the position on X11, or under XWayland with `GDK_BACKEND=x11`, using `xdotool` when it is installed (`GdkX11-4.0` bindings are present on this host, but `xdotool` is not installed and the session is Wayland, so the path is dormant);
- never fails when the position cannot be obtained.

## Verification

Verified 2026-09-22 with `make test`; all fifteen test scripts passed.

- `tests/90-integration-conflicts.sh` gained the upgrade-with-`--replace` case: after installing a second version over the first, exactly one launcher and one manifest remain, the launcher points at the new AppImage, and no icon ends in `.desktop`.
- The earlier replace, add, uninstall-restores, and `explain --json` assertions still pass.
- The two stray icon files were removed from the real desktop; the fix was installed with `make install`.

## Commits

- `002842c` fix: report integration errors, keep the .desktop out of icons, and remember every window geometry
