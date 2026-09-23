# Making the Desktop Overt: Where Applications, Icons, and Parameters Come From

This document exists because AppImage integration is mysterious by default.
Every fact a desktop uses has exactly one source file and a defined precedence.
This document names the source and the command that reveals it.

Live-state facts were verified 2026-09-21 on this Ubuntu 24.04 GNOME Shell 46.0 host.

## The Principle

The desktop never guesses.
For every question it asks, a specification defines an ordered list of places to look and a rule for which answer wins.

```
question  ->  ordered sources  ->  first acceptable answer  ->  provenance
```

When something is wrong, the failure is almost always one of:

1. the fact is in a source the desktop does not read;
2. the fact is in a lower-precedence source that a higher one masks;
3. the fact points at a path or name that no longer resolves.

Each of those is discoverable with the commands below.

## 1. Where Applications Come From

GNOME reads desktop entries through GIO, whose search units are `<data-dir>/applications`, in this order:

| Priority | Directory | Variable and default |
| -------- | --------- | -------------------- |
| 1 | `$XDG_DATA_HOME/applications` | default `$HOME/.local/share/applications` |
| 2..n | `$XDG_DATA_DIRS[i]/applications` | default `/usr/local/share`, `/usr/share` |

Rules:

- The desktop file ID is the path relative to the data directory, with `applications/` removed and `/` replaced by `-`.
- The first directory that yields an ID wins; a later file with the same ID is masked.
- The prefix-to-subdirectory mapping means `kde-foo.desktop` also matches `<dir>/applications/kde/foo.desktop`.
- A file outside an `applications` directory has no ID and is not discoverable by ID.

Reveal it:

| Command | Reveals |
| ------- | ------- |
| `desktop-inspect --path` | the ordered search path |
| `desktop-inspect --all` | every entry on the path, with its ID and source path |
| `desktop-inspect --explain ID` | which file wins for an ID, what it masks, and why |
| `desktop-inspect --locate ID` | just the winning path |

## 2. Where the Parameters Come From

A launcher has exactly one source: its `.desktop` file.
There is no hidden registry and no per-desktop override file.

What can make the effective value differ from the literal file:

| Mechanism | Effect |
| --------- | ------ |
| Localized keys | `Name[de]` overrides `Name` when `LC_MESSAGES` matches |
| Field-code expansion | `%U`, `%F`, `%i`, `%c`, `%k` are expanded at launch time |
| `TryExec` | a missing executable makes the entry disappear |
| `OnlyShowIn` / `NotShowIn` | `$XDG_CURRENT_DESKTOP` decides visibility |
| `Hidden=true` | the entry is suppressed as if deleted |
| Precedence | an entry with the same ID in a higher directory replaces this file entirely |

Reveal it:

| Command | Reveals |
| ------- | ------- |
| `desktop-inspect FILE` | the literal groups, keys, and values |
| `desktop-inspect --locale L FILE` | the value the given locale actually gets |
| `desktop-file-validate FILE` | invalid or contradictory fields |
| `gio launch ID` / `gtk-launch ID` | what the desktop itself would run |
| `xprop WM_CLASS` then click the window | the runtime window class to put in `StartupWMClass` |
| `appimage-integrate windows` | the same class for a running AppImage, from its mount, its X11 client, or the window tree when no window manager has marked the window |
| `lg` (Looking Glass), Windows tab | the `wmclass` of a native Wayland window, which GNOME does not expose to other programs |

## 3. Where Icons Come From

Icon resolution follows the Icon Theme Specification.

The base directories are, in order:

1. `$XDG_DATA_HOME/icons` (default `$HOME/.local/share/icons`)
2. each `$XDG_DATA_DIRS` entry plus `/icons`
3. `/usr/share/pixmaps`

Within a base directory the lookup walks theme directories, then size directories, then contexts, then file names:

```
<base>/<theme>/<size>/<context>/<icon-name>.<extension>
```

- The preferred theme comes from the desktop settings, for example `org.gnome.desktop.interface icon-theme`.
- `hicolor` is the mandatory fallback theme and is always searched.
- A theme may declare `Inherits=` in its `index.theme`, which adds more themes to the chain.
- Size directories are named `<N>x<N>` or `scalable`.
- The `apps` context holds application icons; other contexts hold MIME types, devices, and so on.
- Extensions tried are `.png`, `.svg`, and `.xpm`.
- An `Icon=` value that is an absolute path skips all of this and is used directly.

Three failure modes are common and easy to see:

- The icon is installed in a directory that is not a valid size name, such as `0x0`.
- The launcher uses an absolute path, so themes and scaling do not apply.
- The icon name in the entry does not match the installed file name.

Reveal it:

| Command | Reveals |
| ------- | ------- |
| `desktop-inspect --icon NAME` | every candidate, the winning path, the theme, and the size |
| `desktop-inspect --icon NAME --why` | the themes and directories searched, in order |
| `find ~/.local/share/icons -name 'NAME.*'` | what is actually installed |
| `gtk4-icon-browser` | a graphical view of installed themes, when available |

## 4. Where MIME Associations Come From

The default application for a MIME type is resolved from `mimeapps.list` files, highest priority first:

| Priority | File |
| -------- | ---- |
| 1 | `$XDG_CONFIG_HOME/mimeapps.list` |
| 2 | `$XDG_CONFIG_DIRS[i]/mimeapps.list`, default `/etc/xdg/mimeapps.list` |
| 3 | `$XDG_DATA_HOME/applications/mimeapps.list` |
| 4 | `$XDG_DATA_DIRS[i]/applications/mimeapps.list` |
| 5 | deprecated `defaults.list` in the same data directories |

Rules:

- The group is `[Default Applications]`, with lines `mime/type=id1.desktop;id2.desktop;`.
- The first resolvable desktop ID wins, falling through to the next file.
- `[Removed Associations]` subtracts IDs and `[Added Associations]` adds them.
- `update-desktop-database` writes `mimeinfo.cache` in each `applications` directory; it is a cache, not a source of truth.

Reveal it:

| Command | Reveals |
| ------- | ------- |
| `desktop-inspect --mime TYPE` | the winning default, its desktop file, and the `mimeapps.list` that supplied it |
| `xdg-mime query default TYPE` | the same answer from the reference tool |
| `desktop-inspect --mime TYPE --why` | every `mimeapps.list` searched, in order |

## 5. Where an AppImage's Own Parameters Come From

An AppImage carries its own answers, and they can be read without running it.

| Fact | Source inside the AppImage |
| ---- | -------------------------- |
| desktop entry | the single `*.desktop` file in the payload root |
| application name, icon name, categories, MIME types | the embedded desktop entry |
| icon | `usr/share/icons/hicolor/<size>/apps/<name>`, then a root `<name>.svg`/`.png`, then `.DirIcon` |
| update information | the ELF section `.upd_info` |
| signature | the ELF section `.sha256_sig` |
| payload compression | the SquashFS superblock |

Reveal it:

| Command | Reveals |
| ------- | ------- |
| `appimage-inspect FILE` | container facts, payload listing, and the embedded desktop entry |
| `appimage-inspect --desktop FILE` | only the embedded desktop entry |
| `appimage-inspect --list FILE` | the payload root with sizes and types |
| `appimage-inspect --explain FILE` | what integration would write, and why |

## 6. The Provenance Table

| Fact the desktop uses | Authoritative source | Command that reveals it |
| --------------------- | -------------------- | ----------------------- |
| which applications exist | `*/applications/*.desktop` in `$XDG_DATA_HOME` and `$XDG_DATA_DIRS` | `desktop-inspect --all` |
| which entry wins an ID | directory precedence | `desktop-inspect --explain ID` |
| the label in the menu | `Name` and `Name[locale]` | `desktop-inspect --locale L FILE` |
| the command that runs | `Exec` | `desktop-inspect FILE` |
| whether the launcher is valid | `TryExec` target existence | `desktop-inspect --explain ID` |
| the dock icon match | `StartupWMClass` vs the runtime window class | `appimage-integrate windows`, or `xprop WM_CLASS` |
| the icon rendered | `Icon` resolved through the icon theme | `desktop-inspect --icon NAME` |
| the app offered for a file | `MimeType` plus `mimeapps.list` | `desktop-inspect --mime TYPE` |
| what an AppImage contains | the payload filesystem | `appimage-inspect FILE` |

## 7. Worked Example on This Host

Verified 2026-09-21.

The AppImage MIME types were owned by `appimagelauncher.desktop`:

```
$ for m in application/vnd.appimage application/x-appimage application/x-iso9660-appimage; do
      printf '%-30s %s\n' "$m" "$(xdg-mime query default $m)"
  done
application/vnd.appimage       appimagelauncher.desktop
application/x-appimage         appimagelauncher.desktop
application/x-iso9660-appimage appimagelauncher.desktop
```

The OrcaSlicer application had two active launchers and two backup files:

| File | `Exec` | `Icon` | `StartupWMClass` |
| ---- | ------ | ------ | ---------------- |
| `com.orcaslicer.OrcaSlicer.desktop` | `~/orca-config-backups/run-orcaslicer.sh %F` | `appimagekit_c39..._OrcaSlicer` | `OrcaSlicer` |
| `appimagekit_c39...-OrcaSlicer.desktop` | `~/Applications/OrcaSlicer_...AppImage %F` | `/home/preston/Applications/OrcaSlicer.png` | `com.orcaslicer.OrcaSlicer` |
| `com.orcaslicer.OrcaSlicer.desktop.bak2` | not loaded | | |
| `com.orcaslicer.OrcaSlicer.desktop.bak-142214` | not loaded | | |

The two active entries disagree about both the icon and the window class, which is exactly the kind of hidden state this project is meant to surface.

### Update 2026-09-21

AppImageLauncher was removed at the owner's request; the AppImage MIME definitions it owned were first copied into `$XDG_DATA_HOME/mime/packages/`.

The AppImage MIME default is now `appimage-activator.desktop`, and `desktop-inspect --mime` reports `$XDG_CONFIG_HOME/mimeapps.list` as its source.

The `appimagekit_*` launchers it wrote remain, and `appimage-integrate audit` reports their now-broken `/opt/appimagelauncher.AppDir/...` context actions with the remedy "re-integrate the AppImage".

## 8. The Audit

`appimage-integrate audit` applies the rules above to the real desktop and reports:

- duplicate launchers for one application;
- `Exec` targets that no longer exist or are not executable;
- `Icon` values that are absolute paths or that do not resolve;
- entries missing `StartupWMClass`, with the dock-icon consequence named;
- icon files installed in invalid size directories;
- the current default handler for each AppImage MIME type, and where that default is recorded.

## Verification

- Search and precedence rules: verified 2026-09-21 against the XDG Base Directory Specification, the Icon Theme Specification, and the MIME Applications Specification.
- GIO application behavior: verified 2026-09-21 against GLib `gdesktopappinfo.c`.
- Observed host facts: verified 2026-09-21 by inspecting this host's MIME configuration, application entries, and installed icons.
</content>
