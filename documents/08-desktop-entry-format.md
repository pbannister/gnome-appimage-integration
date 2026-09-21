# The Desktop Entry File Format

This document records the `*.desktop` file format as specified by the
freedesktop.org Desktop Entry Specification, the parts a reader must implement,
and the historical forms still found in the wild.

Live-state facts in this document were verified on 2026-09-21 against the sources listed below.

## Sources

- Desktop Entry Specification, version 1.5 (2020-04-27): <https://specifications.freedesktop.org/desktop-entry-spec/latest/>
- Specification source XML: <https://gitlab.freedesktop.org/xdg/xdg-specs/-/blob/master/desktop-entry/desktop-entry-spec.xml>
- XDG Base Directory Specification, version 0.8 (2021-05-08): <https://specifications.freedesktop.org/basedir-spec/latest/>
- Desktop Menu Specification: <https://specifications.freedesktop.org/menu-spec/latest/>
- Desktop Application Autostart Specification: <https://specifications.freedesktop.org/autostart-spec/latest/>

## File Naming

- Desktop entry files use the `.desktop` extension.
- Files of type `Directory` use the `.directory` extension.
- For applications, the name before the extension should be a valid D-Bus well-known name, for example `org.example.FooViewer.desktop`.
- The name should follow reverse DNS convention, lower case, with the application name run together.
- A dash in a domain label is allowed but not recommended; an underscore is recommended instead.
- A domain label beginning with a digit should be prefixed with an underscore, for example `org._7_zip.Archiver`.

## Desktop File ID

- The desktop file ID is the file path relative to the `$XDG_DATA_DIRS` component it is installed in.
- The `applications/` prefix is removed and `/` is replaced by `-`.
- For example `/usr/share/applications/foo/bar.desktop` has the ID `foo-bar.desktop`.
- If several files share an ID, the first one in the `$XDG_DATA_DIRS` precedence order wins.
- A desktop file outside an `applications/` subdirectory of an `$XDG_DATA_DIRS` component has no ID.

## Basic Format

- The file is encoded in UTF-8.
- The file is a series of lines separated by linefeed characters.
- Case is significant everywhere in the file.
- Lines beginning with `#` and blank lines are comments and must be ignored but preserved on rewrite.
- Comment lines are uninterpreted and may contain any character except linefeed.
- A group header is a line of the form `[groupname]`.
- Group names may contain all ASCII characters except `[`, `]`, and control characters.
- Multiple groups may not share a name.
- All key/value pairs after a group header belong to that group until the next header.
- The required group is `[Desktop Entry]`; it should be the first content in the file.
- An entry is a line of the form `Key=Value`.
- Space before and after the equals sign is ignored; the equals sign is the delimiter.
- Only `A-Za-z0-9-` may appear in key names.
- Keys are case-sensitive; `Name` and `NAME` are different keys.
- A key may appear at most once per group; the same key may appear in different groups.
- Compliant implementations must not drop fields they do not understand.

## Value Types

| Type | Meaning |
| ---- | ------- |
| `string` | ASCII text without control characters |
| `localestring` | user-displayable UTF-8 text |
| `iconstring` | an absolute icon path or a symbolic icon name, not user-displayable |
| `boolean` | the literal `true` or `false` |
| `numeric` | a floating point number as read by `%f` in the C locale |

- The escapes `\s`, `\n`, `\t`, `\r`, and `\\` are supported in `string`, `localestring`, and `iconstring` values.
- They mean space, newline, tab, carriage return, and backslash.

### Lists

- A key may hold several values, written in the specification as `string(s)`.
- Values are separated by a semicolon.
- The final value may be terminated by a semicolon.
- Trailing empty strings must be terminated with a semicolon.
- A literal semicolon inside a value is escaped as `\;`.

## Localized Keys

- Keys of type `localestring` and `iconstring` may carry a locale postfix, as in `Name[fr]`.
- The locale is `lang_COUNTRY.ENCODING@MODIFIER`, where the country, encoding, and modifier parts may be omitted.
- When a postfixed key is present, the same key must also be present without a postfix.
- The encoding part is ignored when matching.
- The reader matches the `LC_MESSAGES` value against candidate keys in this order.

| `LC_MESSAGES` | Candidate keys in order |
| ------------- | ---------------------- |
| `lang_COUNTRY@MODIFIER` | `lang_COUNTRY@MODIFIER`, `lang_COUNTRY`, `lang@MODIFIER`, `lang`, unlocalized |
| `lang_COUNTRY` | `lang_COUNTRY`, `lang`, unlocalized |
| `lang@MODIFIER` | `lang@MODIFIER`, `lang`, unlocalized |
| `lang` | `lang`, unlocalized |

## Recognized Keys

| Key | Type | Required | Applies to |
| --- | ---- | -------- | ---------- |
| `Type` | string | yes | all; `Application`, `Link`, or `Directory` |
| `Version` | string | no | specification version, `1.5` for this version |
| `Name` | localestring | yes | all |
| `GenericName` | localestring | no | all |
| `NoDisplay` | boolean | no | all |
| `Comment` | localestring | no | all |
| `Icon` | iconstring | no | all |
| `Hidden` | boolean | no | all |
| `OnlyShowIn` | string(s) | no | all |
| `NotShowIn` | string(s) | no | all |
| `DBusActivatable` | boolean | no | Application |
| `TryExec` | string | no | Application |
| `Exec` | string | yes unless `DBusActivatable` | Application |
| `Path` | string | no | Application |
| `Terminal` | boolean | no | Application |
| `Actions` | string(s) | no | Application |
| `MimeType` | string(s) | no | Application |
| `Categories` | string(s) | no | Application |
| `Implements` | string(s) | no | Application |
| `Keywords` | localestring(s) | no | Application |
| `StartupNotify` | boolean | no | Application |
| `StartupWMClass` | string | no | Application |
| `PrefersNonDefaultGPU` | boolean | no | Application |
| `SingleMainWindow` | boolean | no | Application |
| `URL` | string | yes for `Link` | Link |

- `Hidden=true` means the entry is deleted for this user and is equivalent to the file not existing.
- `NoDisplay=true` means the entry exists but must not appear in menus.
- `OnlyShowIn` and `NotShowIn` are matched against the colon-separated `$XDG_CURRENT_DESKTOP` value.
- When `OnlyShowIn` is present, the default becomes "do not show".
- Implementations must ignore entries whose `Type` they do not understand.

## Additional Actions

- An entry of type `Application` may define extra invocation modes.
- `Actions` holds a semicolon-separated list of action identifiers.
- Each identifier has a group named `[Desktop Action <identifier>]`.
- An action group without a matching identifier in `Actions` must be ignored.
- An action group requires `Name` and, unless the entry is D-Bus activatable, `Exec`.
- An action group may carry `Icon`.

## The Exec Key

- `Exec` holds a command line: an executable optionally followed by arguments.
- The executable may be a full path or a bare name resolved through `$PATH`.
- The executable name or path may not contain `=`.
- Arguments are separated by spaces and may be quoted in whole with double quotes.
- Inside quotes, `"`, `` ` ``, `$`, and `\` are escaped by a preceding backslash.
- The general string escape rule is applied before the quoting rule.
- Reserved characters that force quoting are space, tab, newline, `"`, `'`, `\`, `>`, `<`, `~`, `|`, `&`, `;`, `$`, `*`, `?`, `#`, `(`, `)`, and `` ` ``.
- Field codes begin with `%` and a letter; a literal percent is `%%`.
- Field codes are expanded once; expanded text is not rescanned.
- A command line containing an unlisted field code is invalid and must not be processed.

| Field code | Expands to |
| ---------- | ---------- |
| `%f` | a single file, even if several are selected |
| `%F` | a list of files |
| `%u` | a single URL |
| `%U` | a list of URLs |
| `%i` | the icon, as two arguments `--icon` and the `Icon` value |
| `%c` | the translated `Name` |
| `%k` | the path to the desktop file |

- Deprecated field codes are `%m`, `%v`, `%d`, `%D`, `%n`, and `%N`; they should be removed and ignored.
- Extensions must be introduced through a new key, never as a new field code.

## Historical and Deprecated Forms

- Pre-1.0 booleans used `0` and `1`; readers should treat them as `false` and `true`.
- Pre-1.0 lists used commas; readers should still accept them.
- `Encoding` is deprecated; values were `UTF-8` and `Legacy-Mixed`.
- `Legacy-Mixed` derives each line's encoding from its locale tag using a fixed language table.
- `Type=MimeType`, `MiniIcon`, `TerminalOptions`, `Protocols`, `Extensions`, `BinaryPattern`, `MapNotify`, `SwallowTitle`, `SwallowExec`, `SortOrder`, and `FilePattern` are deprecated.
- `[KDE Desktop Entry]` and the `.kdelnk` extension are deprecated.
- Extensions to the format use an `X-<PRODUCT>` key prefix or an `[X-<PRODUCT> <GROUP>]` group.

## Consequences for the Reader

The desktop entry reader in this project parses the file into groups, keys, and values.

- Preserve group order and key order so a round trip does not lose unknown fields.
- Preserve comments and blank lines as raw lines so the reader can report them.
- Decode the documented escape sequences and split list values on unescaped semicolons.
- Split localized keys into a base name and an optional locale postfix.
- Select a localized value for a requested locale using the documented matching order.
- Validate required keys and required-in-context keys for `Application`, `Link`, and `Directory` entries.
- Report `Exec` field codes and the deprecated codes that must be removed.
- Accept the historical `0`/`1` booleans and comma separated lists without treating them as errors.

## Verification

- Format facts: verified 2026-09-21 against Desktop Entry Specification version 1.5.
- Locale matching order: verified 2026-09-21 against the specification's locale matching table.
- Exec field codes: verified 2026-09-21 against the specification's Exec key section.
</content>
