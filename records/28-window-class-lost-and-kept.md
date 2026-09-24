# Record: the OrcaSlicer dock icon, lost twice, and the rule that stops it

## Outcome

`StartupWMClass=orca-slicer` is back on the OrcaSlicer launcher and recorded as an explicit
choice, Cura's `UltiMaker-Cura` was restored with it (same failure, same fix), and the class
resolution now refuses to let the embedded entry's guess overwrite a class this tool already
installed.

## What Happened

The launcher had `StartupWMClass=OrcaSlicer` — the value from the AppImage's embedded desktop
entry — where the owner had established that the window's id is `orca-slicer`.  The manifest
said the same, with `startup_wm_class_source=embedded`: the explicit choice was gone, so nothing
was left to restore it from.

The retired manifests in `backup/` date the change:

| Manifest | Written | Class | Source |
| -------- | ------- | ----- | ------ |
| `57434599bed20bde-22.manifest` | 2026-09-23 16:01 | `orca-slicer` | `override` |
| `57434599bed20bde-23.manifest` | 2026-09-24 09:02 | `OrcaSlicer` | `embedded` |

So between those two installs the recorded choice was replaced by the guess.  Two weaknesses
made that possible:

1. **An install trusted the launcher over the record.**  If the launcher holds the embedded
   value again — because an earlier install wrote it, or someone edited it — the class on disk
   is the guess, and the guess was taken as the truth.  Anything that reads the launcher as
   evidence (a re-install, `refresh`) can therefore bake the wrong value in and record it as
   the choice.
2. **`refresh` read the launcher before the record.**  Its order was: `--wm-class`, the running
   window, *the launcher being replaced*, then the record's remembered choice.  A launcher
   holding the guess would overwrite the manifest's `override`, which is the opposite of what
   "remembered choice" means.

Neither could happen with an empty class, which is why the adoption paths looked fine: the bug
needs a *wrong* value to be present, and the embedded entry supplies one.

## The Fix

The class is now chosen in this order:

| Order | Source |
| ----- | ------ |
| 1 | `--wm-class` |
| 2 | the running application, when `--wm-class-from-window` asks for it |
| 3 | a remembered `override` in the manifest |
| 4 | **the launcher this tool already installed**, when it disagrees with the embedded entry |
| 5 | the embedded entry |
| 6 | a plain remembered value |
| 7 | another launcher's class, then a displaced launcher's backup (only when there is none) |

The new step 4 is what the episode turns on: a launcher this tool wrote is better evidence than
the AppImage author's guess, so a hand-set class survives a re-install even if the manifest was
lost.  A launcher written by *another* tool is not evidence and is not adopted here.  Because
the class takes part in deciding which launchers conflict, the detection is repeated after the
class changes, as it already was for the empty-class adoption.

`refresh` also reads the manifest before the launcher now, so a launcher that has lost the class
cannot overwrite the record with the guess.

## The Repair

Both live launchers were re-integrated with the class they should carry, which records it as an
explicit choice:

```
appimage-integrate install --yes --wm-class orca-slicer   <OrcaSlicer AppImage>
appimage-integrate install --yes --wm-class UltiMaker-Cura <Cura AppImage>
```

Cura had the same hole: its class (`UltiMaker-Cura`, read from its running window earlier in
this work) was gone from the record, and its embedded entry names no class at all, so the
launcher had none and the dock had nothing to match.  Cura is not running now; the value comes
from the window reading taken when it was.

All five launchers now carry a class recorded as an override, and a `refresh` leaves them
alone:

```
com.orcaslicer.OrcaSlicer   StartupWMClass=orca-slicer
com.ultimaker.cura          StartupWMClass=UltiMaker-Cura
org.openshot.OpenShot       StartupWMClass=openshot
org.freecad.FreeCAD         StartupWMClass=FreeCAD
nsdt-app                    StartupWMClass=NETGEAR Discovery Tool
```

## Verification

`tests/90-integration-conflicts.sh` gained two sections that fail against the old code:

- a class this tool installed survives a re-install: the launcher is edited to the embedded
  value, a re-install keeps the edited class, reports *kept StartupWMClass=…*, records it as
  `startup_wm_class_source=override`, and a second re-install keeps it again;
- `refresh` trusts the record's choice: after the launcher is edited back to the guess, a
  refresh restores the recorded class rather than adopting the launcher's.

`make test` passes every script.  Live: the OrcaSlicer launcher carries `orca-slicer`, the
manifest says `override`, and both a `refresh --yes` and an install without `--wm-class` leave
it in place.

## If It Happens Again

The class a window really reports is only knowable while the application runs:

```
appimage-integrate windows                      # lists running AppImages and their classes
appimage-integrate install --wm-class-from-window <AppImage>
```

That is the recovery, and it is what the published `AppImage Activator` does from the window
and the context menu.

## Commits

- `3baf79c` Keep a window class this tool installed, and repair two launchers
