# Record: reading `StartupWMClass` from the running application

## Outcome

The tool can now read the class a running AppImage actually reports, and OrcaSlicer was
re-integrated with `orca-slicer` so its dock icon matches. A class set with `--wm-class` is
recorded as an explicit override and survives a re-integration that the embedded entry would
otherwise win back — the regression that produced this episode.

## The Problem

`StartupWMClass` is what lets GNOME match a running window to a launcher, and therefore what
makes the dock show the launcher's icon instead of a generic one. The embedded desktop entry is
the AppImage author's value, and it can simply be wrong. OrcaSlicer's entry says
`StartupWMClass=OrcaSlicer`; the running application's window id is `orca-slicer`. The owner had
established this by hand. Re-integrating then took the embedded value back and the dock icon
regressed.

The open question in `TODO.md` was how to discover the value at all, given that a Wayland
application id needs the compositor.

## What GNOME Exposes, and What It Does Not

- `org.gnome.Shell.Introspect.GetWindows` answers
  `org.freedesktop.DBus.Error.AccessDenied: GetWindows is not allowed` (GNOME Shell 46), and there
  is no shell version method to special-case. A native Wayland window cannot be enumerated by
  another program.
- An X11 or XWayland client *is* visible: `xlsclients -l` prints `Window 0x…`, `Name:`,
  `Command:`, and `Instance/Class:`, and `xprop -id <window> _NET_WM_PID` gives the owning pid.
  The `Instance/Class` pair is the `WM_CLASS` the dock matches on. This works even under a bare
  Xvfb with no window manager, which is what makes it testable.
- The process is always visible: an application started by the AppImage runtime has an executable
  under `/.mount_…` or `/appimage_extracted_…`, and its program name is the id GTK reports as a
  window's application id when the application sets no `Gtk.Application` id (GTK 4.14,
  `gdk/wayland/gdktoplevel-wayland.c`). Payload evidence corroborates OrcaSlicer: `/bin/orca-slicer`,
  and an `/AppRun` that runs native Wayland.

So discovery has two independent sources, one of which always works, and the other of which gives
the exact value the dock uses when there is an X11 window.

## What Was Built

| File | Role |
| ---- | ---- |
| `sources/tools/appimage_integrate.cpp` | `windows`, `--wm-class-from-window`, `/proc` and X11 discovery |
| `sources/integration/appimage_integrator.{h,cpp}` | the explicit-override precedence and its manifest record |
| `tests/94-window-class.sh` | the process fallback, the override rule, and the X11 path under Xvfb |

`appimage-integrate windows [--json]` lists each running AppImage with its windows and processes,
prints the class to use as `--wm-class`, and explains where the value came from. When X11
`WM_CLASS` is available it is preferred, because that is literally the dock's key; otherwise the
program name is offered, with a note saying why it is the right answer. With nothing running it
prints the Looking Glass route (`Alt+F2`, `lg`, Windows tab) rather than a bare empty list.

`appimage-integrate install --wm-class-from-window <AppImage>` resolves the class for one file and
fails with the candidates named when that AppImage is not running. The notice goes to stderr, so
`--json` stays machine-readable.

The precedence for a class is now: `--wm-class`, then an override remembered from an earlier
install, then the embedded entry, then a plain remembered value, then a contradictory launcher,
then a displaced launcher's backup. The manifest records `startup_wm_class_source=override` or
`embedded`, `explain --json` reports it, and `plan` marks an explicit class as kept over the
embedded entry.

## The Defect Found on the Way

The first version of the scan compared `.AppImage` (nine characters) as an eight-character suffix,
so no command line ever matched and `windows` always answered "nothing running". The standalone
probe written to isolate it matched immediately, which located the fault in the comparison rather
than in `/proc` access. The length is now a named constant, `APPIMAGE_SUFFIX_LENGTH`.

## Verification

Verified 2026-09-23 with `make test`; all test scripts passed, including the new
`tests/94-window-class.sh`. That test asserts: `windows --json` always prints an array; a process
under `/.mount_…` whose argv[0] is an AppImage is found and offered as `--wm-class orca-slicer`;
`plan --wm-class-from-window` reads it; an unknown AppImage fails with the running candidates
named; a class installed with `--wm-class` is recorded as `startup_wm_class_source=override` and
survives two later installs that would otherwise restore the embedded value; and, under Xvfb with
`GDK_BACKEND=x11`, a real GTK4 activator window and a mount process are made to claim the *same*
AppImage, so the report and `--wm-class-from-window` must both choose the X11 `WM_CLASS`
(`appimage-activator`) over the program name (`orca-slicer`).

On the live session, `appimage-integrate windows` was exercised against a real running process
inside a `.mount_` directory and against the activator under Xvfb; OrcaSlicer was re-integrated with
`install --wm-class orca-slicer`, and its launcher now carries `StartupWMClass=orca-slicer` with a
matching manifest record. The owner is to confirm the dock icon.

## Follow-Up

- The three launchers that still have no class (Cura, OpenShot, OpenShot-2) can be fixed with
  `install --wm-class-from-window` the next time each application runs; that is now a `TODO.md`
  pending item.
