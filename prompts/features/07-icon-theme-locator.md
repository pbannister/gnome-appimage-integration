# Feature: Icon Theme Locator

## Purpose

The project must answer "where does this icon actually come from?" by resolving an icon
name through the freedesktop Icon Theme Specification, so that an installed launcher's
`Icon=` value can be explained and audited.

## Requirements

- The locator must search `$XDG_DATA_HOME/icons`, each `$XDG_DATA_DIRS` entry plus `/icons`, and `/usr/share/pixmaps`, in that order.
- The locator must ignore a relative path in either environment variable.
- The locator must treat an absolute `Icon=` value as a direct file path and must not search themes for it.
- The locator must try `.png`, `.svg`, and `.xpm` when the name carries no extension.
- The locator must search a preferred theme first, then the theme's `Inherits=` chain, then `hicolor`.
- The locator must resolve a theme inheritance chain with a depth limit and cycle protection.
- The locator must order size directories with `scalable` first, then descending pixel sizes, then unknown names.
- The locator must search the `apps` context before other contexts.
- The locator must return every matching file as a candidate, with its theme, size, and context.
- The locator must report the ordered list of themes and directories it searched.
- The locator must expose a parser for size directory names: `<N>x<N>` returns N, `scalable` returns 0, anything else returns -1.
- The locator must not throw.

## Behavior

- Given an icon installed only under `hicolor/48x48/apps`, a lookup by name returns that path.
- Given the same name in a preferred theme and in `hicolor`, the preferred theme wins.
- Given an absolute path that exists, it is returned unchanged.
- Given an absolute path that does not exist, the lookup reports not found.

## Dependencies

- None.
