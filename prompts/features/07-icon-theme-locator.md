# Feature: Icon Theme Locator

## Purpose

The project must answer "where does this icon actually come from?" by resolving an icon
name through the freedesktop Icon Theme Specification, so that an installed launcher's
`Icon=` value can be explained and audited.

## Requirements

- `ICON-THEME-LOCATOR-R001` — The locator must search `$XDG_DATA_HOME/icons`, each `$XDG_DATA_DIRS` entry plus `/icons`, and `/usr/share/pixmaps`, in that order.
- `ICON-THEME-LOCATOR-R002` — The locator must ignore a relative path in either environment variable.
- `ICON-THEME-LOCATOR-R003` — The locator must treat an absolute `Icon=` value as a direct file path and must not search themes for it.
- `ICON-THEME-LOCATOR-R004` — The locator must try `.png`, `.svg`, and `.xpm` when the name carries no extension.
- `ICON-THEME-LOCATOR-R005` — The locator must search a preferred theme first, then the theme's `Inherits=` chain, then `hicolor`.
- `ICON-THEME-LOCATOR-R006` — The locator must resolve a theme inheritance chain with a depth limit and cycle protection.
- `ICON-THEME-LOCATOR-R007` — The locator must order size directories with `scalable` first, then descending pixel sizes, then unknown names.
- `ICON-THEME-LOCATOR-R008` — The locator must search the `apps` context before other contexts.
- `ICON-THEME-LOCATOR-R009` — The locator must return every matching file as a candidate, with its theme, size, and context.
- `ICON-THEME-LOCATOR-R010` — The locator must report the ordered list of themes and directories it searched.
- `ICON-THEME-LOCATOR-R011` — The locator must expose a parser for size directory names: `<N>x<N>` returns N, `scalable` returns 0, anything else returns -1.
- `ICON-THEME-LOCATOR-R012` — The locator must not throw.

## Behavior

- Given an icon installed only under `hicolor/48x48/apps`, a lookup by name returns that path.
- Given the same name in a preferred theme and in `hicolor`, the preferred theme wins.
- Given an absolute path that exists, it is returned unchanged.
- Given an absolute path that does not exist, the lookup reports not found.

## Dependencies

- None.
