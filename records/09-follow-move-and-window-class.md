# Record: following the moved AppImage, the OrcaSlicer dock icon, and safe re-integration

## Outcome

Three follow-ups from the same session were fixed: the handler now follows an AppImage to its
installed location, OrcaSlicer's dock icon was repaired by recovering its window class, and
re-integrating is confirmed harmless and usable as a repair.

## Following the Moved AppImage

Integrate moves the AppImage into `~/Applications`, but the window kept the old path, so `File`
still showed `Downloads`, and Run now and Inspect failed on a file that was no longer there.

Fixes:

- `explain --json` gained an `installed` field, the path the AppImage will occupy after integration.
- After a successful install the handler sets its path to that location, reloads the description, rebuilds the details rows (so `File` shows the new location), and prints `File is now: …` in the output area.
- Run now and Inspect now operate on the installed path.

## OrcaSlicer's Dock Icon

This was not an icon-resolution problem — the applications list showed the right icon.
The dock matches a running window to a desktop entry by the window's application id or window class.
Our entry had **no `StartupWMClass`**, and the AppImage's embedded entry has none either, so GNOME could not connect the OrcaSlicer window to our launcher.

A class was available: the launcher this tool had displaced during integration carried `StartupWMClass=OrcaSlicer`.

Fixes:

- `plan()` now resolves a missing class in this order: an explicit `--wm-class`, the embedded entry, a previous manifest for the same AppImage, a conflicting launcher, and finally a launcher this tool displaced into its backup directory. Each adoption is reported as a note.
- The chosen class is written to the manifest as `startup_wm_class`, so a later re-integration keeps it.
- OrcaSlicer was repaired on this host with `install --yes --wm-class OrcaSlicer`; its entry now carries `StartupWMClass=OrcaSlicer`, the manifest records `startup_wm_class=OrcaSlicer`, and a following install without the flag preserved it.

If the dock still shows a generic icon for OrcaSlicer, the window's Wayland application id differs from `OrcaSlicer`; it can be read in Looking Glass (`lg`) and passed with `--wm-class`.

## Re-Integrating Is Harmless and Repairs

Re-running install for the same AppImage now demonstrably:

- overwrites its own launcher, icons, and manifest rather than creating a second one;
- refreshes the icon theme cache, so a missing icon is restored;
- keeps the remembered window class;
- backfills a missing class from a conflict or a displaced launcher.

`tests/90-integration-conflicts.sh` now asserts that after installing a class with `--wm-class`, a second install leaves exactly one launcher and one manifest and preserves `StartupWMClass`.

## Verification

Verified 2026-09-22 with `make test`; all fifteen test scripts passed.

- The live OrcaSlicer repair was inspected afterwards: `StartupWMClass=OrcaSlicer`, `Icon=appimage_57434599_OrcaSlicer`, and `startup_wm_class=OrcaSlicer` in the manifest.
- The new test section covers class persistence and idempotent re-installation.

## Commits

- `cd76891` fix: follow the AppImage after install and recover the window class
