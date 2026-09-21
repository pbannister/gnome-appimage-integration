# Where GNOME Looks for Desktop Entry Files

This document records the directories the GNOME desktop searches for `*.desktop`
files, the precedence rules between them, and the observed values on this host.

The directory-selection rules are stable specification facts.
The observed environment values are live state and carry a verification date.

## Sources

- Desktop Entry Specification, file naming and desktop file ID: <https://specifications.freedesktop.org/desktop-entry-spec/latest/>
- XDG Base Directory Specification, version 0.8: <https://specifications.freedesktop.org/basedir-spec/latest/>
- GLib GIO, `gdesktopappinfo.c`, function `desktop_file_dirs_lock`: <https://gitlab.gnome.org/GNOME/glib/-/blob/main/gio/gdesktopappinfo.c>
- Desktop Menu Specification, desktop ID prefix mapping: <https://specifications.freedesktop.org/menu-spec/latest/>
- Desktop Application Autostart Specification: <https://specifications.freedesktop.org/autostart-spec/latest/>
- Flatpak desktop integration: <https://docs.flatpak.org/en/latest/desktop-integration.html>
- snapd desktop menu support: <https://snapcraft.io/docs/desktop-menu-support>

## GNOME Uses GIO

- GNOME Shell and the GNOME application menu enumerate entries through GLib's GIO, not through a GNOME-private directory list.
- GIO builds one ordered list of directories in `desktop_file_dirs_lock` in `gio/gdesktopappinfo.c`.
- GIO appends the literal component `applications` to each data directory with `g_build_filename`, so the search units are `<data-dir>/applications`.
- The order is the precedence order: the first directory that yields an entry wins.

## The GNOME Application Search Path

The application search path, highest priority first, is:

1. `$XDG_DATA_HOME/applications`
2. `$XDG_DATA_DIRS[0]/applications`
3. `$XDG_DATA_DIRS[1]/applications`
4. and so on for every entry of `$XDG_DATA_DIRS`

- `$XDG_DATA_HOME` defaults to `$HOME/.local/share` when unset or empty.
- `$XDG_DATA_DIRS` defaults to `/usr/local/share/:/usr/share/` when unset or empty.
- The list separator is the platform `$PATH` separator, a colon on Linux.
- Relative paths in either variable are invalid and must be ignored.
- A directory that does not exist, or a file that cannot be opened, is skipped without error.

With default values, the path is:

1. `$HOME/.local/share/applications`
2. `/usr/local/share/applications`
3. `/usr/share/applications`

GIO also builds config-directory units from `$XDG_CONFIG_HOME` and `$XDG_CONFIG_DIRS`,
but marks them `is_config` and explicitly refuses to find desktop entries there.
Those units serve configuration lookups such as `mimeapps.list`, not entry discovery.

## Precedence and Masking

- The user data directory is more important than every system data directory.
- Among system data directories, earlier entries are more important than later ones.
- When two files produce the same desktop file ID, the file in the more important directory is used.
- GIO additionally masks a lower-priority file when a same-named file exists in a higher-priority directory.

## Desktop File ID and Subdirectories

- The desktop file ID is the path relative to the data directory, with `applications/` removed and `/` replaced by `-`.
- GIO supports the Menu Specification prefix-to-subdirectory mapping.
- A desktop ID such as `kde-foo.desktop` therefore also matches `/usr/share/applications/kde/foo.desktop`.
- A desktop file that is not under an `applications/` component has no ID and is not discoverable by ID.

## Desktop Environments and Visibility

- `$XDG_CURRENT_DESKTOP` holds a colon-separated list of desktop names set by the login manager.
- `OnlyShowIn` and `NotShowIn` keys are matched against that list.
- GNOME sessions set `$XDG_CURRENT_DESKTOP` to a value containing `GNOME`.

## Autostart Entries

Autostart entries are found by a separate mechanism with a separate path, per the Desktop Application Autostart Specification.

1. `$XDG_CONFIG_HOME/autostart`
2. each `$XDG_CONFIG_DIRS` entry plus `/autostart`

- `$XDG_CONFIG_HOME` defaults to `$HOME/.config`.
- `$XDG_CONFIG_DIRS` defaults to `/etc/xdg`.
- An autostart entry is hidden when a file of the same name in a more important directory contains `Hidden=true`.
- Autostart entries live in these directories directly; the `applications` component is not appended.

## Directories Injected by Packaging Systems

Flatpak and snapd add their export directories to `$XDG_DATA_DIRS` through environment setup,
so GIO sees them without any GNOME-private rule.

- Flatpak adds `$XDG_DATA_HOME/flatpak/exports/share` and `/var/lib/flatpak/exports/share`, so exported desktop files appear under their `applications` subdirectory.
- snapd adds `/var/lib/snapd/desktop`, so confined snap desktop files appear under `/var/lib/snapd/desktop/applications`.
- Distribution vendor directories such as `/usr/share/ubuntu` and `/usr/share/gnome` are added the same way.

## Observed Values on This Host

Verified 2026-09-21 on this Ubuntu 24.04 host running GNOME Shell 46.0.

- `$XDG_DATA_HOME` was unset, so the user directory was `$HOME/.local/share`.
- `$XDG_CURRENT_DESKTOP` was `ubuntu:GNOME`.
- `$XDG_DATA_DIRS` was:

```
/usr/share/ubuntu:/usr/share/gnome:/home/preston/.local/share/flatpak/exports/share:/var/lib/flatpak/exports/share:/usr/local/share/:/usr/share/:/var/lib/snapd/desktop
```

The resulting effective application search path and its observed state was:

| Priority | Directory | Observed 2026-09-21 |
| -------- | --------- | ------------------- |
| 1 | `$HOME/.local/share/applications` | present, 18 entries |
| 2 | `/usr/share/ubuntu/applications` | present |
| 3 | `/usr/share/gnome/applications` | present |
| 4 | `$HOME/.local/share/flatpak/exports/share/applications` | absent |
| 5 | `/var/lib/flatpak/exports/share/applications` | present, 20 entries |
| 6 | `/usr/local/share/applications` | absent |
| 7 | `/usr/share/applications` | present, 129 entries |
| 8 | `/var/lib/snapd/desktop/applications` | present, 32 entries |

Observed autostart directories, verified 2026-09-21:

| Priority | Directory | Observed 2026-09-21 |
| -------- | --------- | ------------------- |
| 1 | `$HOME/.config/autostart` | present, 1 entry |
| 2 | `/etc/xdg/xdg-ubuntu/autostart` | not inspected |
| 3 | `/etc/xdg/autostart` | present, 38 entries |

## Consequences for the Locator

The desktop entry locator in this project mirrors the GIO order.

- Build the application search path from `$XDG_DATA_HOME` and `$XDG_DATA_DIRS`, applying the specification defaults.
- Ignore relative directory entries.
- Append the `applications` component and skip directories that do not exist.
- Build the autostart search path from `$XDG_CONFIG_HOME` and `$XDG_CONFIG_DIRS` with the specification defaults.
- Compute desktop file IDs relative to the matching data directory.
- Resolve an ID through the prefix-to-subdirectory mapping.
- Return the first match and expose the directories that were searched.
- Never hard-code `/usr/share/applications`; it is only the default value of one entry in the path.

## Verification

- Search path assembly: verified 2026-09-21 against `desktop_file_dirs_lock` in GLib `gdesktopappinfo.c`.
- Default values and precedence: verified 2026-09-21 against XDG Base Directory Specification version 0.8.
- ID and prefix mapping: verified 2026-09-21 against the Desktop Entry Specification and the Desktop Menu Specification.
- Observed environment values: verified 2026-09-21 on this host, and checked by the live-state test `tests/50-desktop-search-path-live.sh`.
</content>
