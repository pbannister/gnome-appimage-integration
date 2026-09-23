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

- The AppImage project's own convention and AppImageLauncher's default is `$HOME/Applications`.
- `$HOME/.local/bin` is acceptable for users who want AppImages on `$PATH`.
- A path with spaces is legal and common; every path in an `Exec` line must be quoted.
- Moving or renaming an integrated AppImage breaks its launcher until the entry is rewritten.
- Deleting it leaves a dead launcher unless `TryExec` is present, which makes the launcher hide itself.

Recommended: choose one managed directory, default `$HOME/Applications`, and let the tool move the file there.

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
| `Actions` | optional `Run`, `Update`, `Remove` actions | exposes the lifecycle in the launcher context menu |

The `%U` field code passes selected URLs or files to the application.
`%F` passes files; choose the code that matches the embedded entry.

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
- After the purge, `xdg-mime query filetype` on an AppImage still reports `application/vnd.appimage`, and the default handler for all three AppImage MIME types is `appimage-handler.desktop` from this project.
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
