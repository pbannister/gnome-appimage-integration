# Feature: Command-Line Tools

## Purpose

The project must expose both readers through command-line tools, so a human or a later
integration phase can inspect an AppImage and a desktop entry without writing code.

## Requirements

- `COMMAND-LINE-TOOLS-R001` — The project must provide an `appimage-inspect` executable.
- `COMMAND-LINE-TOOLS-R002` — The project must provide a `desktop-inspect` executable.
- `COMMAND-LINE-TOOLS-R003` — `appimage-inspect <path>` must print the detection outcome, the ELF facts, the payload offset and size, the embedded metadata sections, and the SquashFS superblock.
- `COMMAND-LINE-TOOLS-R004` — `appimage-inspect <path>` must list the payload root entries.
- `COMMAND-LINE-TOOLS-R005` — `appimage-inspect <path>` must print the embedded desktop entry resolved through the Desktop Entry Reader.
- `COMMAND-LINE-TOOLS-R006` — `appimage-inspect --desktop <path>` must print only the embedded desktop entry.
- `COMMAND-LINE-TOOLS-R007` — `appimage-inspect --list <path>` must print only the payload entry listing.
- `COMMAND-LINE-TOOLS-R008` — `appimage-inspect --json <path>` must print the same information as a JSON object.
- `COMMAND-LINE-TOOLS-R009` — `desktop-inspect <path>` must print the parsed groups, keys, and values of one file.
- `COMMAND-LINE-TOOLS-R010` — `desktop-inspect --all` must enumerate the entries found on the application search path.
- `COMMAND-LINE-TOOLS-R011` — `desktop-inspect --locate <identifier>` must resolve one desktop file identifier.
- `COMMAND-LINE-TOOLS-R012` — `desktop-inspect --path` must print the application search path in priority order.
- `COMMAND-LINE-TOOLS-R013` — `desktop-inspect --autostart-path` must print the autostart search path in priority order.
- `COMMAND-LINE-TOOLS-R014` — Both tools must accept `--locale <locale>` and use it for localized value selection.
- `COMMAND-LINE-TOOLS-R015` — Both tools must accept `--help` and print usage.
- `COMMAND-LINE-TOOLS-R016` — Both tools must accept `--version` and print a build-time version string.
- `COMMAND-LINE-TOOLS-R017` — Both tools must write normal output to standard output and diagnostics to standard error.
- `COMMAND-LINE-TOOLS-R018` — Both tools must exit 0 on success and non-zero on error.
- `COMMAND-LINE-TOOLS-R019` — Both tools must exit 2 on a usage error.
- `COMMAND-LINE-TOOLS-R020` — Both tools must not access the network.
- `COMMAND-LINE-TOOLS-R021` — Both tools must not execute the payload of an AppImage.
- `COMMAND-LINE-TOOLS-R022` — `appimage-inspect` must not modify the inspected file.
- `COMMAND-LINE-TOOLS-R023` — `desktop-inspect --all` and `--locate` must only read files.
- `COMMAND-LINE-TOOLS-R024` — Output must be deterministic for the same input.

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
