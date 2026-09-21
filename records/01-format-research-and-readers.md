# Record: format research and readers

## Outcome

The project was created from `00-project-skeleton` and phase 1 was completed.
The AppImage and desktop entry formats were researched from their authoritative specifications and are recorded in `documents/07` through `documents/09`.
The project now reads AppImage containers, their SquashFS payloads, and desktop entry files, and it locates desktop files through the GNOME and XDG search path.
Both readers are reachable through the command-line tools `appimage-inspect` and `desktop-inspect`.

## What Was Built

| Area | Files |
| ---- | ----- |
| AppImage container reader | `sources/appimage/appimage_reader.h`, `sources/appimage/appimage_reader.cpp` |
| SquashFS payload reader | `sources/appimage/squashfs_reader.h`, `sources/appimage/squashfs_reader.cpp` |
| Desktop entry reader | `sources/desktop/desktop_entry_reader.h`, `sources/desktop/desktop_entry_reader.cpp` |
| Desktop entry locator | `sources/desktop/desktop_entry_locator.h`, `sources/desktop/desktop_entry_locator.cpp` |
| Command-line tools | `sources/tools/appimage_inspect.cpp`, `sources/tools/desktop_inspect.cpp` |
| Build | `sources/CMakeLists.txt`, `scripts/program-build.sh`, `scripts/version-generate.sh` |

## Decisions

- The implementation language is C++, per the user's choice, and follows `prompts/flavors/02-cpp-conventions.md`.
- The readers are native libraries with no dependency on libappimage.
- The AppImage container layer depends only on the C and C++ standard libraries.
- The SquashFS layer links `zlib`, `liblzma`, and `libzstd`, which are present on the host.
- SquashFS support covers uncompressed, gzip, xz, and zstd blocks, including files stored in fragments.
- lzma, lzo, and lz4 are reported as unsupported rather than as corrupt data.
- The payload offset is computed with the documented ELF-size algorithm: the maximum of the section-header-table end and the last section end.
- The locator mirrors the GLib GIO order: `$XDG_DATA_HOME/applications` first, then each `$XDG_DATA_DIRS` entry plus `/applications`.
- The locator normalizes a trailing slash, as GLib does.
- The build tree is generated output under `dataflow.out/build/`, so it stays out of version control.
- `make build` now builds the readers and tools; the site page set moved to `make site`.
- The version is generated from git state by `scripts/version-generate.sh` per `prompts/03-conventions.md` §6.2.

## Verification

Verified 2026-09-21 with `make test` from a clean tree; all tests passed.

| Test | Tier | Result |
| ---- | ---- | ------ |
| `tests/00-skeleton.sh` | portable | pass |
| `tests/01-site-build.sh` | portable | pass |
| `tests/02-test-run-log.sh` | portable | pass |
| `tests/10-desktop-entry-reader.sh` | portable | pass |
| `tests/20-desktop-entry-locator.sh` | portable | pass |
| `tests/30-appimage-reader.sh` | portable | pass |
| `tests/40-squashfs-payload-reader.sh` | tool-gated on `mksquashfs` | pass for gzip, xz, zstd, and uncompressed blocks |
| `tests/50-desktop-search-path-live.sh` | live-state | PASS, 5 existing directories, 190 entries, 0 warnings |
| `tests/60-command-line-tools.sh` | integration | pass |

Additional evidence:

- The integration test computes the ELF size independently with `od` and confirms that `appimage-inspect` reports the same payload offset.
- The SquashFS test compares extracted bytes against the source files, including a 192000-byte file that spans multiple data blocks and a fragment.
- The live-state test confirms the search path matches the derivation from `$XDG_DATA_HOME` and `$XDG_DATA_DIRS` on this GNOME Shell 46.0 host.

## Commits

- `c1656d9` chore: import 00-project-skeleton baseline
- `78df9de` docs: record AppImage, desktop entry, and search-path formats
- `3440508` feat: read desktop entry files and locate them on the XDG search path
- `62972a7` feat: read AppImage containers and their SquashFS payloads
- `03475d6` feat: add appimage-inspect and desktop-inspect command-line tools

## Next Tasks

1. Decide the open questions in `TODO.md`, especially the icon policy and whether integration installs a desktop file.
2. Implement phase 2: install a desktop entry for an AppImage under `$XDG_DATA_HOME/applications` and verify it appears in the GNOME application menu.
3. Consider extracting `.DirIcon` or resolving `Icon=` through the XDG icon theme for the installed entry.
