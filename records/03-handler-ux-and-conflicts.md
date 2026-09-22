# Record: handler feedback and integration conflicts

## Outcome

Four problems from the user story were fixed.

- Run once now reports "Starting {name}: process {pid}" for several seconds instead of starting silently.
- Inspect now shows a full report instead of an empty box.
- Integrate now shows exactly what was written.
- Integrating a second version of an already-integrated application now presents choices instead of a dead-end error.

## What Was Wrong, and What Changed

| Symptom | Cause | Fix |
| ------- | ----- | --- |
| Run once gave no feedback during a slow start | the handler replaced itself with the AppImage, so nothing was left to report | fork, `setsid`, and exec in the child; the parent shows a notice with the child process id for several seconds (`notify-send -t 5000`, else `zenity --info --timeout=5`, else standard output) |
| Inspect showed an empty box | `zenity --text-info` reads its body from standard input or `--filename`; the report was passed through `--text`, which is only the label | the report is rendered in-process and handed over with `--filename` |
| Integrate did "something" with no summary | the success dialog only named the desktop id | a scrollable report now shows the launcher, AppImage, icon, manifest, notes, warnings, launcher contents, and the list of actions |
| A new FreeCAD version failed with a useless error | the tool had no concept of an application that is already integrated | conflicts are detected and named, and the user is offered Replace existing, Add alongside, or Cancel |

## Conflict Handling

- Detection matches an existing launcher against the new AppImage by normalized `Name` (so `FreeCAD` and `FreeCAD (1)` match), `X-AppImage-Name`, `StartupWMClass`, and the AppImage file stem.
- Each conflict records its origin: `this tool`, `AppImageLauncher` (it still carries `X-AppImage-Identifier`), or `unknown`.
- `--replace` moves each displaced launcher into `$XDG_DATA_HOME/gnome-appimage-integration/backup/` and records it in the manifest; `uninstall` moves it back.
- `--add` keeps the existing launcher and installs under a suffixed desktop file id such as `org.example.Probe-2.desktop`.
- A launcher this tool already owns at the same desktop id is treated as an in-place upgrade, not a conflict; its old manifest is retired so `list` does not show a stale entry.
- The failure message names each conflicting launcher and states both choices.

## The Real FreeCAD Case

Observed 2026-09-22 on this host, read-only.

- The new download was `$HOME/Downloads/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage`, not executable, so a right-click offers the handler.
- FreeCAD 1.1.1 had been integrated by this tool: manifest `84b4aad2fb518980`, launcher `org.freecad.FreeCAD.desktop`.
- The AppImageLauncher-era launcher `appimagekit_f55dc857fd6b44ae58db0c1e87cf797c-FreeCAD.desktop` was still present.
- `appimage-integrate plan` on the new download now reports one conflicting launcher, names it as `AppImageLauncher`, and offers `--replace` and `--add`.
- The upgrade was deliberately left for the owner to choose:
  - `appimage-integrate install --replace "$HOME/Downloads/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage"`
  - `appimage-integrate install --add "$HOME/Downloads/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage"`

## Verification

Verified 2026-09-22 with `make test`; all fourteen test scripts passed.

- `tests/90-integration-conflicts.sh` is new and covers: a refusing plan that names the conflict and both choices, `--replace` backing up the displaced launcher, `uninstall` restoring it, `--add` creating `org.example.Probe-2.desktop`, and `explain` showing the embedded entry.
- `tests/70-integration-sandbox.sh` now also covers `run --detached` reporting the start and the process id.
- The real files were only read; `plan` and `explain` were run without changing anything.

## Commits

- `080334e` feat: give the handler feedback and offer choices on integration conflicts
