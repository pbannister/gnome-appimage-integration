# Feature: Command-Line Tools

## Purpose

The project must expose both readers through command-line tools, so a human or a later
integration phase can inspect an AppImage and a desktop entry without writing code.

## Requirements

- The project must provide an `appimage-inspect` executable.
- The project must provide a `desktop-inspect` executable.
- `appimage-inspect <path>` must print the detection outcome, the ELF facts, the payload offset and size, the embedded metadata sections, and the SquashFS superblock.
- `appimage-inspect <path>` must list the payload root entries.
- `appimage-inspect <path>` must print the embedded desktop entry resolved through the Desktop Entry Reader.
- `appimage-inspect --desktop <path>` must print only the embedded desktop entry.
- `appimage-inspect --list <path>` must print only the payload entry listing.
- `appimage-inspect --json <path>` must print the same information as a JSON object.
- `desktop-inspect <path>` must print the parsed groups, keys, and values of one file.
- `desktop-inspect --all` must enumerate the entries found on the application search path.
- `desktop-inspect --locate <identifier>` must resolve one desktop file identifier.
- `desktop-inspect --path` must print the application search path in priority order.
- `desktop-inspect --autostart-path` must print the autostart search path in priority order.
- Both tools must accept `--locale <locale>` and use it for localized value selection.
- Both tools must accept `--help` and print usage.
- Both tools must accept `--version` and print a build-time version string.
- Both tools must write normal output to standard output and diagnostics to standard error.
- Both tools must exit 0 on success and non-zero on error.
- Both tools must exit 2 on a usage error.
- Both tools must not access the network.
- Both tools must not execute the payload of an AppImage.
- `appimage-inspect` must not modify the inspected file.
- `desktop-inspect --all` and `--locate` must only read files.
- Output must be deterministic for the same input.

## Behavior

- `appimage-inspect` on a valid type 2 AppImage prints the type, the payload offset, and the embedded entry's `Name`.
- `appimage-inspect` on a text file prints an error to standard error and exits non-zero.
- `desktop-inspect` on the specification example prints the `[Desktop Entry]` group and both action groups.
- `desktop-inspect --path` prints at least one directory and never fails on a machine with no desktop files.
- `--help` prints usage and exits 0.

## Dependencies

- `03-appimage-reader.md`
- `04-desktop-entry-reader.md`
- `05-desktop-entry-locator.md`
