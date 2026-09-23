# AppImage Desktop Integration: Best Practice

This document answers the end-user question: "I downloaded an AppImage; what now?"

It records the recommended best practice for using an AppImage, the two user flows, what
"desktop integration" actually writes, and why each piece is needed.

Live-state facts in this document were verified on 2026-09-21 on this Ubuntu 24.04 GNOME Shell 46.0 host.

## Sources

- AppImage specification, working draft: <https://github.com/AppImage/AppImageSpec/blob/master/draft.md>
- AppImage documentation, desktop integration: <https://docs.appimage.org/reference/desktop-integration.html>
- AppImage documentation, AppDir: <https://docs.appimage.org/reference/appdir.html>
- AppImage runtime documentation: <https://github.com/AppImage/type2-runtime>
- AppImageLauncher reference implementation: <https://github.com/TheAssassin/AppImageLauncher>
- appimaged (background daemon): <https://github.com/AppImage/appimaged>
- GearLever (GUI manager): <https://github.com/mijorus/gearlever>
- Desktop Entry Specification: <https://specifications.freedesktop.org/desktop-entry-spec/latest/>
- Icon Theme Specification: <https://specifications.freedesktop.org/icon-theme-spec/latest/>
- XDG Base Directory Specification: <https://specifications.freedesktop.org/basedir-spec/latest/>
- MIME Applications Specification: <https://specifications.freedesktop.org/mime-apps-spec/latest/>
- Shared MIME-info Database Specification: <https://specifications.freedesktop.org/shared-mime-info-spec/latest/>

## The Problem

An AppImage is a single self-contained executable file.
Running it is easy once it is executable.
Living with it is not: the file is a stranger on the system.

- The desktop does not know the application exists, so it never appears in the application menu.
- The dock cannot match the running window to a launcher, so it shows a generic icon.
- The file manager will not run an untrusted executable, so a double-click does nothing useful.
- The AppImage has no stable home, so users accumulate mysterious files in `Downloads`.

"Desktop integration" is the name for fixing each of those.

## Two Flows, Not One

An AppImage user wants exactly one of two things, and the user interface should ask which.

| Flow | What it means | Side effects |
| ---- | ------------- | ------------ |
| Run once | Try the application without committing to it | Nothing is written; the file stays where it is |
| Integrate | Make the application a real desktop application | The AppImage moves to a stable home, a desktop entry and icons are written, and the app appears in the menu |

AppImageLauncher implements exactly this prompt on first execution, offering "Integrate and run" or "Run once".

## Where an AppImage Should Live

An integrated AppImage must not stay in `Downloads`.
The desktop entry stores an absolute path to it, so the path becomes a contract.

There is no XDG directory for AppImages. The XDG Base Directory Specification defines
`$XDG_DATA_HOME` for data, `$XDG_CONFIG_HOME` for configuration, `$XDG_CACHE_HOME`,
`$XDG_STATE_HOME`, and `$XDG_RUNTIME_DIR`; it defines no directory for executables, and an
AppImage is an executable. Two conventions compete:

- `$HOME/Applications` is what AppImageLauncher uses and what the AppImage project's own tooling
  documented, so it is the de-facto standard and the default here.
- `$XDG_DATA_HOME/AppImages` (that is, `~/.local/share/AppImages`) is the defensible XDG-shaped
  choice, because an AppImage is user data. Pass `--install-dir "$XDG_DATA_HOME/AppImages"` to
  use it. Whichever is chosen, the directory may be anywhere the user can write.

- `$HOME/.local/bin` is acceptable for users who want AppImages on `$PATH`.
- A path with spaces is legal and common; every path in an `Exec` line must be quoted.
- The managed directory is created if it does not exist, by the step that places the AppImage.
- Removing the managed directory leaves every launcher pointing at a path that no longer exists.
  `appimage-integrate list` marks those records `[MISSING]`, and `audit` reports the missing
  `Exec` and `TryExec` targets. Re-integrating the AppImage from wherever it now is repairs the
  launcher and recreates the directory; the AppImage keeps its identifier, because the
  identifier is derived from the file's contents rather than its path.
- Moving or renaming an integrated AppImage breaks its launcher until the entry is rewritten.
- Deleting it leaves a dead launcher unless `TryExec` is present, which makes the launcher hide itself.

Recommended: choose one managed directory, default `$HOME/Applications`, and let the tool move the file there.

#### Which Button the Window Suggests

The tool reports `version` (this file) and `installed_version` plus `version_relation`
(`newer`, `older`, `same`, or `unknown`) in `explain --json`, comparing the newest version among
the launchers that already represent the application. The activator uses that to point at the
likely next action, following the GNOME HIG's suggested-action style:

| Situation | First view | After Integrate |
| --------- | ---------- | --------------- |
| same version, same file | `Close` suggested: nothing to do | — |
| same version, a different file | `Integrate` suggested | `Replace existing`, name unchanged |
| newer version | `Integrate` suggested | `Replace existing`, name unchanged |
| older version | `Close` suggested, with the versions shown in Status | `Add alongside`, with the version appended to the name |
| more than one launcher exists | `Integrate` suggested | `Add alongside`, with the version appended to the name |

An older file never replaces a newer installation on a click: Close is the likely action, Status
prints "This AppImage is older than the installed version" with both versions, and integrating
anyway keeps the newer launcher and writes the new one under a name that carries the version.

## The Things Integrate Can Report

`appimage-integrate` says in one line which of them a run is, under `this-run:` in the plan and
in the `mode` field of `explain --json`, and the graphical activator shows the same line in its
details block and its Status tab:

| Situation | `this-run:` | What happens |
| --------- | ----------- | ------------ |
| nothing represents the application yet | `new integration` | the launcher, icons, and record are written |
| it is already complete | `properly integrated` | nothing: the launcher runs this file, the file is in the managed directory, and the record exists |
| the launcher runs this file, but the AppImage is not in the managed directory | `update the launcher in place` | the launcher and record are rewritten and the AppImage is placed in the managed directory |
| the same AppImage is somewhere else now | `repair the launcher (the AppImage is not where it was)` | the AppImage returns to the managed directory and the launcher's `Exec` and `TryExec` are rewritten |
| a different AppImage wants a launcher this tool owns | `replace an existing launcher` (with `--replace`) or `add alongside as <id>` (with `--add`) | the old launcher is backed up and restored by `uninstall`, or kept and a distinct launcher is added |
| a different AppImage wants it, and no choice was given | `another launcher already represents this application` | nothing yet: the plan stops with the list of launchers and the two choices |

The distinction between an update, a repair, and a different build is the AppImage's identifier:
it is a hash of the embedded entry, the file size, and the payload offset, so the same AppImage
keeps its identifier when it moves, while a different build does not.

## What Integration Writes

Best practice is a small, fixed set of files, all under the user's home, all reversible.

| Target | Purpose |
| ------ | ------- |
| `$HOME/Applications/<name>.AppImage` | the stable home of the payload |
| `$XDG_DATA_HOME/applications/<id>.desktop` | the launcher the menu and dock read |
| `$XDG_DATA_HOME/icons/hicolor/<size>/apps/<icon-name>.<ext>` | the icon the launcher and dock render |
| `$XDG_DATA_HOME/mime/packages/<id>.xml` | optional, only when the AppImage ships MIME definitions |
| `$XDG_CONFIG_HOME/mimeapps.list` | only when the launcher is made the default for a MIME type |

With the default values of the XDG variables, that is:

```
~/.local/share/applications/<id>.desktop
~/.local/share/icons/hicolor/256x256/apps/<icon-name>.png
```

### The desktop entry

The launcher is generated from the `.desktop` file embedded in the AppImage, with the
fields that must change patched, and with provenance keys added.

| Key | Value written | Why |
| --- | ------------- | --- |
| `Type` | `Application` | makes it a launchable application |
| `Name`, `GenericName`, `Comment`, `Categories`, `MimeType`, `Keywords` | copied from the embedded entry | preserves the author's metadata |
| `Exec` | `"<absolute AppImage path>" [args] %U` | runs the AppImage, not an internal path |
| `TryExec` | `<absolute AppImage path>` | lets the desktop hide a dead launcher |
| `Icon` | the installed icon name, not a path | lets icon themes and scaling work |
| `StartupWMClass` | the application's window class | lets the dock match the window to the launcher |
| `StartupNotify` | `true` | lets the launcher show a busy state while starting |
| `Terminal` | `false` | an AppImage launcher is a GUI application |
| `X-AppImage-Identifier` | a stable identifier for this AppImage | lets update and remove find it later |
| `Actions` | `AppImage-Activator;Remove-AppImage;` with a `[Desktop Action …]` group for each | exposes the lifecycle in the launcher context menu |

The `%U` field code passes selected URLs or files to the application.
`%F` passes files; choose the code that matches the embedded entry.

### The launcher's context menu

GNOME Shell builds the right-click menu of an application icon from the entry's `Actions=`, using
each action group's `Name=` as the label and running its `Exec=`. So the actions are where a
downloaded AppImage can offer its own lifecycle:

| Action | Label | Runs |
| ------ | ----- | ---- |
| `AppImage-Activator` | `AppImage Activator` | `appimage-integrate handle <AppImage>`, which opens the graphical activator on that file |
| `Remove-AppImage` | `Remove this AppImage` | `appimage-integrate uninstall --identifier <id>` |

The shell also adds its own **App Details** item to that menu, which opens GNOME Software. That
item is not the entry's business and cannot be suppressed from a `.desktop` file: GNOME Shell 46
shows it whenever GNOME Software is installed, whatever the application is
(`js/ui/appMenu.js`, `_updateDetailsVisibility()`: the item is visible when
`lookup_app('org.gnome.Software.desktop')` finds an app). For an AppImage from a download, GNOME
Software then shows the distribution's package, which is a different thing entirely. The way to
remove it is to remove GNOME Software, or to hide the item with a GNOME Shell extension.

### The icon

Three sources exist inside an AppImage, in this order of preference.

1. The themed icons under `usr/share/icons/hicolor/<size>/apps/<icon-name>.<ext>`.
2. The root icon named by `Icon=`, for example `openshot-qt.svg` or `cura-icon.png`.
3. `.DirIcon` in the payload root, which is the AppDir's own icon and should be a 256x256 PNG.

An `Icon=` value that is an absolute path works but defeats theming and breaks when the file moves.
Best practice is to install the icon under the hicolor theme and set `Icon=` to the icon name.

### The window class

`StartupWMClass` is the single most commonly missing piece.
Without it, GNOME cannot prove that a running window belongs to a launcher, so the dock shows
a generic icon and a second, anonymous dock entry.

The correct value is the application's WM class, which is visible at runtime:

```
xprop WM_CLASS          # then click the running window
```

or, for a running application, `lg` (Looking Glass) in GNOME shows the window's `wmclass`.

On Wayland the shell matches on the window's **application id**, not on a WM class. GTK takes
that id from the `Gtk.Application` id when one is set and from the program name otherwise
(GTK 4.14, `gdk/wayland/gdktoplevel-wayland.c`). Whichever value is in play, it must equal the
launcher's file name: a window whose application id is `us.example.Thing` does not match
`thing.desktop`, and the dock then shows a generic icon even though `StartupWMClass` is set.
A handler that wants the file name `appimage-activator.desktop` therefore sets no
`Gtk.Application` id and uses `appimage-activator` as its program name.

### Naming a launcher the user can tell apart

`Name=` is the menu label, and it may be set independently of the AppImage: several launchers can
run different versions of one application, so `--name` overrides the plain `Name=` with something
like `OrcaSlicer 2.4.2` while the embedded entry keeps supplying everything else. Conflict
detection still uses the embedded name, so a renamed launcher is still recognised as the same
application. Localised `Name[xx]` lines in the embedded entry are kept as the author wrote them.

### When two AppImages want the same launcher

The launcher identifier comes from the embedded `.desktop` file name, which is a property of
the application, not of the file, so two different AppImages can want the same identifier.
The tool never resolves that silently.

| Situation | What happens |
| --------- | ------------ |
| the same file is integrated again | in-place upgrade: the launcher, icon, and record are rewritten, and the previous window class is remembered |
| a different AppImage wants an identifier this tool already owns | reported as a conflict, named as `this tool (a different AppImage for the same identifier)`, and refused until a policy is chosen |
| a launcher written by another tool represents the application | reported as a conflict with its origin, and refused until a policy is chosen |
| `--replace` | the displaced launcher is backed up and restored by `uninstall`; the identifier changes hands |
| `--add` | the existing launcher, its record, and its icon are all left untouched, and this AppImage is installed alongside as a distinct launcher |

A record is keyed by identifier, so an added copy needs a record of its own:
`--add` writes `<id>-N.desktop`, the record `<identifier>-N`, and the icon `<icon-name>-N`.
Without that, the new record would overwrite the record that still describes the launcher
it was asked to keep, and the kept launcher would look like a foreign one.

`appimage-integrate audit` reports a launcher whose record disagrees with the AppImage it
actually runs, which is the state a silent takeover used to leave behind.

## The Double-Click Problem

Desktop environments deliberately do not execute arbitrary downloaded files.
On GNOME, Nautilus will not run an AppImage on double-click by default.

The mechanism that makes a double-click useful is a MIME handler:

1. The shared MIME database already defines the AppImage types by magic bytes and by the `*.AppImage` glob.
2. A handler is a `.desktop` file whose `MimeType=` lists those types and whose `Exec=` runs a tool with `%f`.
3. The handler is made the default for the type in `$XDG_CONFIG_HOME/mimeapps.list`.

The handler's `Name=` is what the right-click menu shows, so it is user-visible and must not
clash with another project's name. This project calls it **AppImage Activator**:
`appimage-activator.desktop`, icon `appimage-activator.svg`, and `StartupWMClass=appimage-activator`
in step with the window's program name. It was called `appimage-handler` before the rename, and
`handler install` moves those files out of the way and carries their recorded previous defaults
into the new record.

On this host the system MIME definitions are:

| MIME type | Meaning | Detection |
| --------- | ------- | --------- |
| `application/vnd.appimage` | type 2 AppImage | ELF magic plus `0x41 0x49 0x02` at offset 8 |
| `application/x-iso9660-appimage` | type 1 AppImage | ELF magic plus `0x41 0x49 0x01` at offset 8 |
| `application/x-appimage` | legacy/untyped alias | used by older tools |

The current default on this host was `appimagelauncher.desktop` for all three types, verified 2026-09-21.

## FUSE and the Extract-and-Run Fallback

A type 2 AppImage is mounted through FUSE at run time.
Older AppImages need `libfuse2`; the modern static runtime does not.
In containers, minimal systems, or systems without FUSE, mounting fails.

Fallbacks that always work:

- Pass `--appimage-extract-and-run` to the AppImage.
- Set `APPIMAGE_EXTRACT_AND_RUN=1` in the environment.

These extract the payload to a temporary directory and run it from there.
They are slower and use more temporary disk, so they are a fallback, not the default.

## Security

- An AppImage is executable code from the internet; integrate only sources you trust.
- The AppImage specification defines sentinels that suppress self-integration: `$XDG_DATA_HOME/appimagekit/no_desktopintegration`, `/usr/share/appimagekit/no_desktopintegration`, `/etc/appimagekit/no_desktopintegration`, a running `appimaged` process, or a non-empty `$DESKTOPINTEGRATION`.
- A background daemon that integrates everything it finds, such as `appimaged`, has been criticised for the file I/O, the lack of consent, and the attack surface it creates.
- Integration written by a user-level tool needs no root privileges; nothing here should require `sudo`.

## The Tool Landscape

| Tool | Model | Notes |
| ---- | ----- | ----- |
| AppImageLauncher | MIME handler plus dialog | the de-facto standard; moves to `~/Applications`, adds Update and Remove actions |
| appimaged | background daemon | scans directories and integrates without asking; the AppImageLauncher README calls it a security hazard |
| GearLever | GUI application | manages a library of AppImages and their entries |
| libappimage | library | the integration algorithm underneath several tools |
| This project | explicit CLI plus opt-in handler | every step can be printed, planned, and reversed |

## Observed Reality on This Host

Verified 2026-09-21; this is the state the project's `audit` command is designed to expose.

- AppImageLauncher 3.0.0-beta-2 was installed and was the default handler for all three AppImage MIME types.
- Five AppImages were integrated: OpenShot, FreeCAD, OrcaSlicer, UltiMaker Cura, and Raspberry Pi Imager.
- Two FreeCAD launchers existed, from identifiers `f55dc857fd6b44ae58db0c1e87cf797c` and `cd74351f9d906bd3afef3d151af73e79`.
- OrcaSlicer had an active `com.orcaslicer.OrcaSlicer.desktop` whose `Exec` pointed at a wrapper script outside `~/Applications`, alongside an `appimagekit_c39dc95119a81b9d7e5153a6feb79bfb-OrcaSlicer.desktop` whose `Icon` was the absolute path `/home/preston/Applications/OrcaSlicer.png`, plus two `.bak` files.
- One icon was installed into a size directory literally named `0x0`, which no theme lookup will find.
- Icons for OpenShot and FreeCAD were split across `16x16`, `32x32`, `48x48`, `64x64`, `128x128`, `256x256`, `512x512`, and `scalable`, which is the correct hicolor practice.

### Update 2026-09-21: AppImageLauncher was removed

The owner asked for AppImageLauncher to be uninstalled because its presence made the desktop state harder to read.

- The package `appimagelauncher` 3.0.0-beta-2 was purged with `apt-get purge`.
- It owned `/usr/share/mime/packages/appimage.xml`, the AppImage MIME definitions, so a copy was installed at `$XDG_DATA_HOME/mime/packages/appimage.xml` first and `update-mime-database` was run.
- After the purge, `xdg-mime query filetype` on an AppImage still reports `application/vnd.appimage`, and the default handler for all three AppImage MIME types is this project's `appimage-activator.desktop` (called `appimage-handler.desktop` before the rename).
- The launchers AppImageLauncher had written remain in `$XDG_DATA_HOME/applications/appimagekit_*.desktop`; their `Exec` still launches the AppImages, but their `Remove` and `Update` context actions point at `/opt/appimagelauncher.AppDir`, which no longer exists.
- `appimage-integrate audit` reports each of those broken actions, so the remaining cleanup is visible rather than silent.
- The AppImageLauncher-created MIME packages under `$XDG_DATA_HOME/mime/packages/appimagekit_*.xml` were kept, because they define application-specific types such as FreeCAD documents.

## Best-Practice Checklist

1. Never integrate from `Downloads`; move the AppImage to a managed directory first.
2. Keep the AppImage's executable bit set.
3. Generate the desktop entry from the embedded one; do not hand-write the metadata.
4. Quote the absolute path in `Exec` and add a matching `TryExec`.
5. Install the icon under `hicolor` and reference it by name.
6. Set `StartupWMClass` from the running application, and keep it current.
7. Add `Actions` for remove and update, so the lifecycle is reachable from the launcher.
8. Record provenance, so the entry can be audited and reversed.
9. Make the double-click handler explicit, opt-in, and restorable.
10. Prefer one managed directory and one launcher per application; detect duplicates.

## Verification

- Specification facts: verified 2026-09-21 against the AppImage specification, the Desktop Entry Specification, and the Icon Theme Specification.
- Handler and MIME facts: verified 2026-09-21 against this host's `/usr/share/mime/packages/appimage.xml` and `xdg-mime query default`.
- Installed layout: verified 2026-09-21 by inspecting this host's `~/.local/share/applications` and `~/.local/share/icons`.
</content>
