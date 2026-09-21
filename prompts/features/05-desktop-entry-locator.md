# Feature: Desktop Entry Locator

## Purpose

The project must find the `*.desktop` files that the GNOME desktop would find, using the
same directory order and identifier rules, so that a reader can resolve a desktop file by
identifier and can enumerate installed entries.

The search rules implemented here are recorded in `documents/09-desktop-file-search-paths.md`.

## Requirements

- The locator must build the application search path from `$XDG_DATA_HOME` and `$XDG_DATA_DIRS`.
- The locator must use `$HOME/.local/share` when `$XDG_DATA_HOME` is unset or empty.
- The locator must use `/usr/local/share:/usr/share` when `$XDG_DATA_DIRS` is unset or empty.
- The locator must ignore a relative path in either variable.
- The locator must append the `applications` component to each data directory.
- The locator must place the user data directory before every system data directory.
- The locator must preserve the order of `$XDG_DATA_DIRS`.
- The locator must skip a directory that does not exist and must not report it as an error.
- The locator must build the autostart search path from `$XDG_CONFIG_HOME` and `$XDG_CONFIG_DIRS`.
- The locator must use `$HOME/.config` when `$XDG_CONFIG_HOME` is unset or empty.
- The locator must use `/etc/xdg` when `$XDG_CONFIG_DIRS` is unset or empty.
- The locator must not append the `applications` component to an autostart directory.
- The locator must expose the ordered list of directories it searched.
- The locator must compute a desktop file identifier from a path relative to a data directory.
- The locator must remove the `applications/` prefix and replace `/` with `-` when computing an identifier.
- The locator must return no identifier for a file outside an `applications` component.
- The locator must resolve an identifier by searching the path in order and returning the first match.
- The locator must support the prefix-to-subdirectory mapping, so that an identifier `kde-foo.desktop` also matches `<dir>/applications/kde/foo.desktop`.
- The locator must enumerate entries with a first-match-wins rule for duplicate identifiers.
- The locator must not require any environment variable to be set.
- The locator must be read-only and must not create, modify, or delete any file.

## Behavior

- Given no environment overrides, the application search path starts with `$HOME/.local/share/applications` and ends with the `applications` subdirectory of every `$XDG_DATA_DIRS` entry.
- Given a temporary `$XDG_DATA_HOME` with one desktop file, enumeration returns exactly that entry.
- Given the same identifier in two data directories, resolution returns the file in the earlier directory.
- Given an identifier with a dash, resolution also finds the nested subdirectory form.
- Given an environment with a relative `$XDG_DATA_DIRS` entry, that entry is skipped.

## Dependencies

- None.
- The locator supplies paths to the AppImage Reader and the Desktop Entry Reader, but does not depend on them.
