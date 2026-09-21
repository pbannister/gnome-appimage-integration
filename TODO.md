# TODO

## Open Questions

* [ ] decide whether the AppImage reader must support type 1 (ISO 9660) payloads or only type 2.
      — The spec defines both; type 1 is legacy and the reader currently reports the type without listing its payload.
* [ ] decide the icon policy for a future integration phase: extract `.DirIcon` into a cache, or resolve `Icon=` through the XDG icon theme.
* [ ] decide whether desktop integration installs a desktop file under `$XDG_DATA_HOME/applications` or only reports what it would install.
* [ ] decide whether the reader should verify the `.sha256_sig` signature when the section is non-empty.
* [ ] improve the human-oriented documents in `documents/`.
* [ ] read the tool-universe sources in `documents/02-tool-universe.md`.

## Pending

* [ ] document the GNOME and XDG desktop file search paths in `documents/09-desktop-file-search-paths.md`.
* [ ] implement the desktop entry reader (`sources/desktop/desktop_entry_reader.*`).
* [ ] implement the desktop entry locator (`sources/desktop/desktop_entry_locator.*`).
* [ ] implement the AppImage container reader (`sources/appimage/appimage_reader.*`).
* [ ] implement the SquashFS payload reader (`sources/appimage/squashfs_reader.*`).
* [ ] implement the command-line tools (`sources/tools/`).
* [ ] add the portable tests and the tool-gated and live-state tests under `tests/`.
* [ ] wire `make build` and `make test` to the C++ build.

## Recently Completed

* [x] create the project from `00-project-skeleton` and record the baseline commit.
* [x] document the AppImage format in `documents/07-appimage-format.md`.
* [x] document the desktop entry format in `documents/08-desktop-entry-format.md`.
* [x] register the new documents in `documents/README.md`.
