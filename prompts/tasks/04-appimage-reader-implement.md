# Task: Implement the AppImage Reader feature

## TASK-DESCRIPTION
* Create `sources/appimage/appimage_reader.h`.
* Create `sources/appimage/appimage_reader.cpp`.
* Create `sources/appimage/squashfs_reader.h`.
* Create `sources/appimage/squashfs_reader.cpp`.
* Create the portable test `tests/30-appimage-reader.sh`.
* Create the tool-gated test `tests/40-squashfs-payload-reader.sh`.
* Implement the requirements in `prompts/features/03-appimage-reader.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/03-appimage-reader.md`.
* Format facts and the payload offset algorithm: `documents/07-appimage-format.md`.
* The SquashFS reader may use the host `zlib`, `liblzma`, and `libzstd` libraries.
* The test fixture is a real ELF from the host with the AppImage magic written at offset 8 and a SquashFS image appended at the computed payload offset.

## TASK-FILES
- `sources/appimage/appimage_reader.h` — new
- `sources/appimage/appimage_reader.cpp` — new
- `sources/appimage/squashfs_reader.h` — new
- `sources/appimage/squashfs_reader.cpp` — new
- `tests/30-appimage-reader.sh` — new
- `tests/40-squashfs-payload-reader.sh` — new
