# Feature: Desktop Entry Reader

## Purpose

The project must read a `*.desktop` file into a structured, lossless representation so
that the application name, icon, launch command, and action list are available without
re-parsing the file.

The format requirements implemented here are recorded in `documents/08-desktop-entry-format.md`.

## Requirements

- `DESKTOP-ENTRY-READER-R001` — The reader must parse a desktop entry from a filesystem path and from an in-memory string.
- `DESKTOP-ENTRY-READER-R002` — The reader must interpret input as UTF-8.
- `DESKTOP-ENTRY-READER-R003` — The reader must preserve the order of groups and the order of keys within a group.
- `DESKTOP-ENTRY-READER-R004` — The reader must preserve comment lines and blank lines as raw lines associated with their position.
- `DESKTOP-ENTRY-READER-R005` — The reader must preserve unknown keys and unknown groups without dropping them.
- `DESKTOP-ENTRY-READER-R006` — The reader must report a syntax error with the one-based source line number for a malformed line.
- `DESKTOP-ENTRY-READER-R007` — The reader must reject a line outside any group that is neither a comment, a blank line, nor a group header.
- `DESKTOP-ENTRY-READER-R008` — The reader must split a group header `[name]` into its name and reject an unterminated header.
- `DESKTOP-ENTRY-READER-R009` — The reader must split a key/value pair at the first `=` and ignore whitespace around the delimiter.
- `DESKTOP-ENTRY-READER-R010` — The reader must validate key names against the permitted ASCII set `A-Za-z0-9-`.
- `DESKTOP-ENTRY-READER-R011` — The reader must split a localized key `Base[locale]` into the base name and the locale postfix.
- `DESKTOP-ENTRY-READER-R012` — The reader must decode the escape sequences `\s`, `\n`, `\t`, `\r`, and `\\`.
- `DESKTOP-ENTRY-READER-R013` — The reader must decode `\;` as a literal semicolon inside a list value and must not split on it.
- `DESKTOP-ENTRY-READER-R014` — The reader must split a list value on unescaped semicolons and must honour the trailing-semicolon termination rule.
- `DESKTOP-ENTRY-READER-R015` — The reader must parse a boolean as `true` or `false`, and must additionally accept the historical `0` and `1` values.
- `DESKTOP-ENTRY-READER-R016` — The reader must parse a numeric value as a C-locale floating point number.
- `DESKTOP-ENTRY-READER-R017` — The reader must select a localized value for a requested locale using the matching order in `documents/08-desktop-entry-format.md`.
- `DESKTOP-ENTRY-READER-R018` — The reader must expose the unlocalized value when no locale-specific value matches.
- `DESKTOP-ENTRY-READER-R019` — The reader must expose the declared type of an entry as `Application`, `Link`, `Directory`, or unknown.
- `DESKTOP-ENTRY-READER-R020` — The reader must validate that `Type` and `Name` are present.
- `DESKTOP-ENTRY-READER-R021` — The reader must validate that `Exec` is present for an `Application` entry unless `DBusActivatable` is `true`.
- `DESKTOP-ENTRY-READER-R022` — The reader must validate that `URL` is present for a `Link` entry.
- `DESKTOP-ENTRY-READER-R023` — The reader must report a missing required key as a validation error, separately from a syntax error.
- `DESKTOP-ENTRY-READER-R024` — The reader must expose the `Actions` list and resolve each identifier to its `[Desktop Action <id>]` group.
- `DESKTOP-ENTRY-READER-R025` — The reader must ignore an action group whose identifier is absent from `Actions` and must report it as a warning.
- `DESKTOP-ENTRY-READER-R026` — The reader must expose the `Exec` field codes present in a value.
- `DESKTOP-ENTRY-READER-R027` — The reader must report the deprecated field codes `%m`, `%v`, `%d`, `%D`, `%n`, and `%N` when present.
- `DESKTOP-ENTRY-READER-R028` — The reader must not modify the input file.
- `DESKTOP-ENTRY-READER-R029` — The reader must not throw an exception out of its public interface.

## Behavior

- Given the specification's example file, parsing yields two action groups in declaration order.
- Given a file with a `Name[sr_YU]` key and other localized `Name` keys, a request for `sr_YU` returns the `Name[sr_YU]` value.
- Given a requested locale with no matching key, the reader returns the unlocalized value.
- Given a file missing `Name`, parsing succeeds and validation reports the missing required key.
- Given a malformed line, parsing fails with the correct line number and no partial result is returned.
- Reading the same file twice yields equal results.

## Dependencies

- None.
