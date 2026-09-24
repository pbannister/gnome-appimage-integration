# Record: the activator's dock icon, and naming an alongside launcher

## Outcome

Two reports were fixed. The activator window now carries the application id GNOME matches the
launcher against, so the dock shows the activator icon, and the conflict prompt gained a
prefilled `Name:` field whose value becomes `Name=` in the launcher.

## The Dock Icon: the Window's Application Id

The icon was installed, the theme cache was fresh, and `Icon=appimage-activator` resolved. What
was missing was the *match* between the running window and the launcher.

GNOME matches a Wayland window to a launcher by the window's **application id**. GTK 4.14 sets
that id in `gdk_wayland_toplevel_publish_app_id` (`gdk/wayland/gdktoplevel-wayland.c:874`) as:

```c
app_id = wayland_toplevel->application.application_id;
if (app_id == NULL)
  app_id = g_get_prgname ();
if (app_id == NULL)
  app_id = "GTK Application";
```

The activator passed `application_id="us.bannister.appimage-activator"`, so the window's app id
was that invented name. GNOME's heuristic then looks for `us.bannister.appimage-activator`, then
`us.bannister.appimage-activator.desktop`, then a launcher whose `StartupWMClass` is
`us.bannister.appimage-activator` — and our launcher is `appimage-activator.desktop` with
`StartupWMClass=appimage-activator`. No match, so the dock showed a generic icon for a window it
could not attribute.

Files and their lines were read from the installed version (GTK 4.14.5) rather than guessed.
The fix is to set no `Gtk.Application` id at all, so the app id falls back to the program name,
which is already `appimage-activator` and already equals the launcher's file name. The
`StartupWMClass` and the X11 `WM_CLASS` stay as they were; both were verified on X11 under
Xvfb (`WM_CLASS = "appimage-activator", "appimage-activator"`) and the window still starts with
no GApplication warnings when the id is absent.

`tests/95-activator-ui.sh` now derives the program name from the script, compares it with the
generated launcher's file name, and fails if any `application_id=` reappears.

## Naming a Launcher

`install --name NAME` writes `Name=` in the launcher. The embedded entry keeps supplying every
other field, and conflict detection still uses the *embedded* name, so a renamed launcher is
still recognised as the same application. A control character in the requested name becomes one
space, because a newline would break the desktop entry; if nothing printable is left, the
embedded name is used and a warning is printed. Localised `Name[xx]` lines are kept as the
author wrote them, and the plan says so when they exist.

In the graphical activator, the conflict prompt now shows a `Name:` label and a single-line
entry to the left of the buttons, prefilled with the AppImage's name. Add alongside and Replace
existing pass the field through as `--name`, so a second launcher can say `OrcaSlicer 2.4.2`
instead of repeating `OrcaSlicer`. The field is shown only when that choice is offered: with no
conflict, Integrate simply installs, and there is nothing to tell apart.

## Tests

- `tests/90-integration-conflicts.sh`: `install --add --name "Probe App 9.9.10"` writes that
  `Name=` to the new launcher, leaves the other launchers' `Name=` alone, and `explain --json`
  still reports the embedded name.
- `tests/95-activator-ui.sh`: under `xvfb-run`, with a stub tool that records its arguments, the
  real window is built and driven: the field starts hidden, Integrate reveals it prefilled with
  the AppImage's name, and Add alongside passes the typed value as `--name`.

## Found While Verifying

`~/Applications` no longer exists on this host; the whole directory now sits at
`~/Downloads/Applications`. Every integrated launcher still has `Exec=~/Applications/…`,
so those launchers are broken until they are re-integrated from the new paths. Nothing in this
project moved them — the tests work only inside `mktemp` sandboxes with a redirected `HOME` — but
it is worth re-running `appimage-integrate audit` and `install` once the directory is settled.

## Verification

Verified 2026-09-23 with `make test`; all fifteen test scripts passed, including the Xvfb-driven
window test. `appimage-integrate plan --add --name "OrcaSlicer 2.4.2" <AppImage>` printed
`Name=OrcaSlicer 2.4.2` against a real AppImage, and the installed activator was checked for the
program name, the launcher id, the window class, and icon resolution.

## Commits

- `0d29085` Match the activator window's application id to its launcher
