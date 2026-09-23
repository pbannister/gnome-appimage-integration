# Desktop Entry Parameters and What the Desktop Does With Them

This document is the operational reference for every key a `*.desktop` file may carry.
It records what each key does, which desktop component consumes it, and how to inspect the
effective value.

`documents/08-desktop-entry-format.md` defines the file format and the parser rules.
This document defines the meaning and the effect.
Live-state facts were verified 2026-09-21 on this Ubuntu 24.04 GNOME Shell 46.0 host.

## Sources

- Desktop Entry Specification version 1.5: <https://specifications.freedesktop.org/desktop-entry-spec/latest/>
- Desktop Menu Specification: <https://specifications.freedesktop.org/menu-spec/latest/>
- Icon Theme Specification: <https://specifications.freedesktop.org/icon-theme-spec/latest/>
- MIME Applications Specification: <https://specifications.freedesktop.org/mime-apps-spec/latest/>
- AppImage desktop integration keys: <https://docs.appimage.org/reference/desktop-integration.html>
- AppImageLauncher `X-AppImage-*` keys: <https://github.com/TheAssassin/AppImageLauncher>

## Components That Consume a Desktop Entry

| Component | Reads | Effect |
| --------- | ----- | ------ |
| Application menu (GNOME Shell, `gmenu`) | `Name`, `Icon`, `Categories`, `NoDisplay`, `Hidden`, `OnlyShowIn`, `NotShowIn` | the entry appears, is grouped, or is hidden |
| Launcher (`gio launch`, `gtk-launch`) | `Exec`, `TryExec`, `Path`, `Terminal`, `DBusActivatable` | the application starts |
| Dock and window tracker | `StartupWMClass`, `StartupNotify` | the window is matched to the launcher and shows its icon |
| File manager and MIME database | `MimeType`, `Exec` field codes | the application is offered for a file type |
| Launcher context menu | `Actions` plus `[Desktop Action ...]` groups | extra commands are offered |
| Icon loader | `Icon` | an icon name or absolute path is resolved |

## Core Identity Keys

| Key | Type | Required | Meaning and effect |
| --- | ---- | -------- | ------------------ |
| `Type` | string | yes | `Application`, `Link`, or `Directory`; determines which other keys are legal |
| `Version` | string | no | the specification version the entry conforms to, `1.5` today; ignored by most desktops |
| `Name` | localestring | yes | the label in the menu and dock; `--name` overrides it, so several launchers for one application can be told apart by version |
| `GenericName` | localestring | no | a category label such as `Web Browser` |
| `Comment` | localestring | no | the tooltip |
| `Keywords` | localestring(s) | no | extra search terms; a semicolon list |
| `Categories` | string(s) | no | menu placement, from the registered category list; one main category is recommended |
| `Icon` | iconstring | no | an icon name resolved through the icon theme, or an absolute path |
| `NoDisplay` | boolean | no | `true` keeps the entry out of menus while it stays usable for file associations |

Notes:

- `Categories` is validated by `desktop-file-validate`, which warns when more than one main category is present because the application may then appear twice.
- `Icon` with an absolute path bypasses theming and scaling, and breaks if the file moves; a name installed under `hicolor` is the durable form.
- `Name` and `GenericName` and `Comment` may carry locale postfixes such as `Name[de]`.

## Visibility Keys

| Key | Type | Meaning and effect |
| --- | ---- | ------------------ |
| `Hidden` | boolean | `true` means "deleted for this user"; equivalent to the file not existing |
| `OnlyShowIn` | string(s) | show only in the listed desktop environments |
| `NotShowIn` | string(s) | hide in the listed desktop environments |

Matching is against the colon-separated `$XDG_CURRENT_DESKTOP` value, for example `ubuntu:GNOME`.
When `OnlyShowIn` is present the default becomes "do not show".

## Launch Keys

| Key | Type | Required | Meaning and effect |
| --- | ---- | -------- | ------------------ |
| `Exec` | string | yes unless `DBusActivatable=true` | the command line to run |
| `TryExec` | string | no | if this executable is missing, the entry is treated as absent and hidden |
| `Path` | string | no | the working directory for the launched program |
| `Terminal` | boolean | no | `true` launches the program inside a terminal emulator |
| `DBusActivatable` | boolean | no | `true` activates the application over D-Bus instead of `Exec` |
| `StartupNotify` | boolean | no | `true` asks the desktop to show a starting indicator |
| `StartupWMClass` | string | no | the window class that belongs to this launcher |
| `PrefersNonDefaultGPU` | boolean | no | prefer a non-default GPU on dual-GPU systems |
| `SingleMainWindow` | boolean | no | the application can only have one main window |

Notes:

- `Exec` is the key that integration must rewrite, because the embedded value usually names a program inside the payload rather than the AppImage itself.
- `TryExec` is the key that makes a stale launcher disappear instead of appearing broken.
- `StartupWMClass` is what makes the dock show the right icon for a running AppImage; without it the window is anonymous. The embedded value is the AppImage author's guess, so it can disagree with the class the application actually reports: read the real one with `appimage-integrate windows` and set it with `--wm-class`, which then outranks the embedded entry.
- `Path` inside an AppImage is usually wrong, because the working directory should be the mount point or the AppImage's own directory.

## Exec Field Codes

| Code | Expands to |
| ---- | ---------- |
| `%f` | a single file path |
| `%F` | a list of file paths |
| `%u` | a single URL |
| `%U` | a list of URLs |
| `%i` | the icon as two arguments, `--icon` and the `Icon` value |
| `%c` | the translated `Name` |
| `%k` | the path of the desktop file itself |

Rules:

- A literal percent sign is `%%`.
- Deprecated and ignored: `%m`, `%v`, `%d`, `%D`, `%n`, `%N`.
- Field codes are expanded once; expanded text is not rescanned.
- An unlisted field code makes the command line invalid and it must not be processed.
- Arguments containing spaces, quotes, or other reserved characters are quoted with double quotes; `"`, `` ` ``, `$`, and `\` are escaped with a backslash.
- For an AppImage, the whole absolute AppImage path is quoted and followed by the entry's own field code.

## Association Keys

| Key | Type | Meaning and effect |
| --- | ---- | ------------------ |
| `MimeType` | string(s) | the MIME types the application can open |

- The list is a semicolon-separated set, for example `model/stl;application/x-amf;`.
- A MIME type becomes openable by this application only when the entry is visible to the MIME database; `update-desktop-database` refreshes the cache in each `applications` directory.
- The default application for a MIME type is recorded separately in `mimeapps.list`, not in the desktop entry.

## Action Keys

| Key | Type | Meaning and effect |
| --- | ---- | ------------------ |
| `Actions` | string(s) | identifiers of additional commands offered in the launcher context menu |

Each identifier has a group `[Desktop Action <identifier>]` containing:

| Key | Type | Required | Meaning |
| --- | ---- | -------- | ------- |
| `Name` | localestring | yes | the context-menu label |
| `Exec` | string | yes unless D-Bus activated | the command to run |
| `Icon` | iconstring | no | the context-menu icon |

AppImageLauncher uses actions for its lifecycle:

```
Actions=AppImageLauncher-Remove-AppImage;AppImageLauncher-Update-AppImage;
[Desktop Action AppImageLauncher-Remove-AppImage]
Name=Delete this AppImage
Exec=/path/to/remove "%f"
```

## Compatibility Keys

| Key | Type | Meaning and effect |
| --- | ---- | ------------------ |
| `Implements` | string(s) | interfaces the application implements, such as `org.freedesktop.FileManager1` |
| `URL` | string | required for `Type=Link` |

## Extension and Provenance Keys

Keys beginning with `X-` are extensions and are ignored by desktops that do not know them.
They are the correct place to record provenance.

| Key | Defined by | Meaning |
| --- | ---------- | ------- |
| `X-AppImage-Name` | AppImage | application name, used to relate versions |
| `X-AppImage-Version` | AppImage | bundled application version |
| `X-AppImage-Arch` | AppImage | bundled architecture |
| `X-AppImage-Identifier` | AppImageLauncher | hash identifying the AppImage file |
| `X-AppImage-Old-Icon` | AppImageLauncher | the icon name the entry was created with |
| `X-AppImageLauncher-Version` | AppImageLauncher | the version of the tool that wrote the entry |
| `X-GNOME-FullName` | GNOME | historical full name |
| `X-GNOME-Autostart-*` | GNOME | autostart condition and delay |
| `X-KDE-*` | KDE | KDE-specific behavior |
| `X-AppInstall-Package` | appstream | the distribution package the entry was generated from |

## Deprecated Keys

These are recognized but should not be produced.

`Encoding`, `MiniIcon`, `TerminalOptions`, `Protocols`, `Extensions`, `BinaryPattern`,
`MapNotify`, `SwallowTitle`, `SwallowExec`, `SortOrder`, `FilePattern`, `Dev`, `FSType`,
`MountPoint`, `ReadOnly`, `UnmountIcon`, `ServiceTypes`, `DocPath`, `InitialPreference`.

Historical values that readers must still accept:

- Boolean `0` and `1` for `false` and `true`.
- Comma-separated lists before specification 1.0.
- The `[KDE Desktop Entry]` group name and the `.kdelnk` extension.

## How to Inspect Every Parameter

| Question | Command |
| -------- | ------- |
| What does this file literally say? | `desktop-inspect FILE` |
| What value does a locale actually get? | `desktop-inspect --locale de FILE` |
| Which entry wins for an identifier, and what does it mask? | `desktop-inspect --explain ID` |
| What are the valid values for a field? | `desktop-file-validate FILE` |
| Which entry does the menu use for this identifier? | `gio launch ID`, or `gtk-launch ID` |
| What icon does a name resolve to? | `desktop-inspect --icon NAME` |
| Which application opens a MIME type, and from which file? | `desktop-inspect --mime TYPE` |
| What are all the parameters in the wild on this host? | `desktop-inspect --all --fields` |

## Verification

- Key meanings: verified 2026-09-21 against Desktop Entry Specification version 1.5.
- Field codes: verified 2026-09-21 against the specification's Exec key section.
- AppImage keys: verified 2026-09-21 against the AppImage desktop-integration documentation and this host's AppImageLauncher-generated entries.
- Component behavior: verified 2026-09-21 on GNOME Shell 46.0.
</content>
