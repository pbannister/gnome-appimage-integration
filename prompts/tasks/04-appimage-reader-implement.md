# Task: Implement the AppImage Reader feature

## TASK-DESCRIPTION
- Create: `sources/appimage/appimage_reader.h`
- Create: `sources/appimage/appimage_reader.cpp`
- Create: `sources/appimage/squashfs_reader.h`
- Create: `sources/appimage/squashfs_reader.cpp`
- Create: `tests/30-appimage-reader.sh`
- Create: `tests/40-squashfs-payload-reader.sh`
The reader identifies the AppImage type, computes the payload offset, validates the SquashFS superblock, and reads payload entries, and implements the requirements in `prompts/features/03-appimage-reader.md`.
The fixture is a real host ELF with the AppImage magic written at offset 8 and a SquashFS image appended at the computed payload offset.

## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/appimage/appimage_reader.h`
2. `sources/appimage/appimage_reader.cpp`
3. `sources/appimage/squashfs_reader.h`
4. `sources/appimage/squashfs_reader.cpp`
5. `tests/30-appimage-reader.sh`
6. `tests/40-squashfs-payload-reader.sh`

## TASK-CONTEXT
<note>
Format facts and the payload offset algorithm are in `documents/07-appimage-format.md`. The SquashFS reader may use the host `zlib`, `liblzma`, and `libzstd` libraries, and the test skips cleanly when `mksquashfs` is absent.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/appimage/appimage_reader.h` |
| create | `sources/appimage/appimage_reader.cpp` |
| create | `sources/appimage/squashfs_reader.h` |
| create | `sources/appimage/squashfs_reader.cpp` |
| create | `tests/30-appimage-reader.sh` |
| create | `tests/40-squashfs-payload-reader.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and the reader's payload offset matches the offset the AppImage runtime reports for the same file.

## TASK-FEATURES
- `prompts/features/03-appimage-reader.md`

## TASK-ACCEPTANCE
- `APPIMAGE-READER-R003`
- `APPIMAGE-READER-R008`
- `APPIMAGE-READER-R013`
- `APPIMAGE-READER-R016`
- `APPIMAGE-READER-R021`
- `APPIMAGE-READER-R023`
- `APPIMAGE-READER-R024`
- `APPIMAGE-READER-R025`

OUTPUT: the six complete files in the order listed, ending with the `VERIFICATION:` line.
