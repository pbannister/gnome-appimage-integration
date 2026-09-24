# Record: `refresh`, and the two window-class discoveries behind it

## Outcome

`appimage-integrate refresh` re-writes every recorded launcher from its embedded entry, so a
launcher written by an older template gains the new keys in one command; all eight launchers on
this host now carry the `AppImage Activator` context action. The three launchers that had no
`StartupWMClass` were given one read from their running application: Cura with
`install --wm-class-from-window`, OpenShot and OpenShot-2 with `refresh --wm-class-from-window`.
Two defects stood in the way, and both are fixed: a real AppImage's payload could not be traced
back to its file, and a window that no window manager had marked was invisible.

## A Real AppImage Does Not Name Its File Any More

The window class feature identified an AppImage by walking a process's ancestry for a command
line that ends in `.AppImage`. That works for the runtime process and for a program started with
an AppImage path as `argv[0]`, which is what the test did, but not for a real application:
`setsid Cura.AppImage` gives a payload process whose parent is `systemd --user`, with no ancestor
that names the file at all. The runtime process that does name it is a sibling, not a parent.

The authoritative source is the environment the runtime leaves behind:

```
APPIMAGE=~/Applications/UltiMaker-Cura-5.13.0-linux-X64.AppImage
APPDIR=/tmp/.mount_UltiManBieKJ
```

`/proc/<pid>/environ` is read for `APPIMAGE` after the command line, so the runtime process,
a program started with an AppImage path, and a reparented payload all resolve. The FUSE mount
also carries the file name in its type (`fuse.UltiMaker-Cura-…-X64.AppImage`), but it gives only
the base name, so it was not used.

## GNOME Exposes X11 Windows Only Through the Window Manager

`xlsclients -l` lists exactly the windows a window manager has marked with `WM_STATE`, which is
why it works on the real GNOME session but finds nothing on a bare Xvfb: Qt and GTK set WM_CLASS
themselves, and no manager turns that into a client-list entry. `xwininfo -root -tree` lists every
mapped window with its title and its instance/class pair, without any cooperation:

```
0x20000e " Untitled Project [HD 720p 30 fps] - OpenShot Video Editor": ("openshot-qt" "openshot")  1096x823+0+0
```

`windows` now falls back to the tree when `xlsclients` yields nothing, and both are filtered by
`xprop -id <window> _NET_WM_PID` and `APPIMAGE`, so only windows that belong to a running
AppImage are reported. The fallback is exercised by the test with a `PATH` that has `xwininfo`
and `xprop` but no `xlsclients`.

The tree also settled what `StartupWMClass` has to contain: it is the **class** half of the pair.
The Qt 5.15 xcb plugin builds WM_CLASS from `-name`/`RESOURCE_NAME`/`argv[0]` for the instance
and from `QCoreApplication::applicationName()` for the class
(`QXcbIntegration::wmClass()`), so OpenShot's window is instance `openshot-qt`, class `openshot`
— it calls `setApplicationName('openshot')` and runs through a payload binary named
`openshot-qt`. `code`/`Code` on this host is the same shape, and GNOME's `code.desktop` says
`StartupWMClass=Code`. An earlier attempt on this host had written `openshot-qt`, the instance,
which would never have matched.

## OpenShot Does Not Start on This Session

`OpenShot-v3.3.0-x86_64.AppImage` segfaults immediately under the real GNOME/Wayland session
(the bundle has no Wayland Qt plugin, so it uses xcb through XWayland), and it segfaults on plain
Xvfb as well. With `LIBGL_ALWAYS_SOFTWARE=1 QT_XCB_GL_INTEGRATION=none` on Xvfb it starts and
reaches "Qt Ready / Angular Ready", which is how its window was finally read. It also arrived
here without its executable bit (`-r--r--r--`), which the tool restores when it rewrites the
launcher. Neither fact was fixed — the AppImage is upstream's — but the second one is what made
the first crash message so unhelpful at first.

## Refresh

`refresh` walks the records, opens each AppImage, reads the launcher it is about to replace, and
re-plans with the record's own choices:

| Preserved | Why |
| --------- | --- |
| `Name=` from the launcher | a version or a hand-chosen label is not in the embedded entry |
| desktop id and icon name from the record | re-deriving them could rename an installed launcher |
| `StartupWMClass` from the launcher, the record, or the running application | the class is a runtime fact, not an entry fact |
| the record's identifier | two launchers for one AppImage must keep two records |

The last line was a real defect the test caught: without an identifier override, refreshing a
launcher recorded as `<id>-2` re-derived the plain `<id>` from the file, wrote over the first
record, and left the second launcher with no record at all — after which `install` reported it as
"this tool (launcher with no record)". `integration_options_o::identifier_override` now keeps the
record's identity, and `refresh_own_launchers` stops another launcher this tool wrote for the
same file from being reported as a competing claim.

The class is only taken from an explicit source: `--wm-class`, or `--wm-class-from-window` when a
window is found, or otherwise what the launcher already has, or the record. Nothing is invented,
so a launcher that has a class keeps it unless a flag says otherwise. A record whose AppImage is
gone is printed as skipped and makes the command exit non-zero; `balena-etcher-electron.desktop`
is in exactly that state on this host.

## Verification

Verified 2026-09-23 with `make test`; all test scripts passed, including the new
`tests/96-refresh.sh` and the extended `tests/94-window-class.sh`. The refresh test installs two
launchers for one AppImage, reduces them to an older shape (no `Actions=`, no `StartupWMClass`,
no action group), and asserts that a dry run writes nothing, that `refresh` restores both the
action and the action group in both launchers without calling them a conflict and without making
a third launcher, that a class in the launcher is kept when no option is given, that `--wm-class`
reaches every launcher, that a class missing from a launcher is restored from the record, that
`--wm-class-from-window` falls back when nothing is running, that a renamed launcher keeps its
name, that `--json` reports what was written, and that a missing AppImage is reported and skipped.
The window test now also proves the tree fallback by hiding `xlsclients` from the `PATH`.

On the live session, `refresh` rewrote all eight launchers, `audit` no longer reports a missing
`StartupWMClass` on any of them, and the two OpenShot launchers show `StartupWMClass=openshot`
read from the running window `0x20000e`. Cura's class was read from its own window
(`UltiMaker-Cura`) before the feature was extended. The owner is to confirm the dock icons for
Cura and OpenShot.

## Commits

- `2726fac` Add refresh, and trace a payload back to its AppImage by APPIMAGE
- `a672039` Record: refresh and the live window classes
