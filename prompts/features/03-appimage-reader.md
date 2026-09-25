# Feature: AppImage Reader

## Purpose

The project must read an AppImage file without executing it and expose the container
structure, the embedded metadata, and the payload filesystem, so that a later phase can
integrate the packaged application into the GNOME desktop.

The format requirements implemented here are recorded in `documents/07-appimage-format.md`.

## Requirements

- `APPIMAGE-READER-R001` — The reader must accept an ordinary filesystem path and must not require the `.AppImage` extension.
- `APPIMAGE-READER-R002` — The reader must validate the ELF identification bytes before any other interpretation.
- `APPIMAGE-READER-R003` — The reader must distinguish four detection outcomes: not an ELF, an ELF without the AppImage magic, a type 1 image, and a type 2 image.
- `APPIMAGE-READER-R004` — The reader must classify the magic value `0x41 0x49 0x01` as type 1 and `0x41 0x49 0x02` as type 2.
- `APPIMAGE-READER-R005` — The reader must classify an ELF whose offset-8 magic is neither value as an unrecognized image type.
- `APPIMAGE-READER-R006` — The reader must report the ELF class (32-bit or 64-bit), the data encoding (little-endian or big-endian), the machine identifier, and the entry point.
- `APPIMAGE-READER-R007` — The reader must convert multi-byte ELF fields from the file byte order rather than the host byte order.
- `APPIMAGE-READER-R008` — The reader must compute the payload offset as the maximum of the section-header-table end and the end of the last section.
- `APPIMAGE-READER-R009` — The reader must compute the section-header-table end as `e_shoff + (e_shentsize * e_shnum)`.
- `APPIMAGE-READER-R010` — The reader must compute the last section end as the last section header's `sh_offset + sh_size`.
- `APPIMAGE-READER-R011` — The reader must report the payload size as the file size minus the payload offset.
- `APPIMAGE-READER-R012` — The reader must report an error when the payload offset is not smaller than the file size.
- `APPIMAGE-READER-R013` — The reader must locate the ELF sections `.upd_info` and `.sha256_sig` through the section-header string table and report each one's offset and size.
- `APPIMAGE-READER-R014` — The reader must expose the update information text when `.upd_info` is present.
- `APPIMAGE-READER-R015` — The reader must report whether a present `.sha256_sig` section is entirely zero padding or carries signature data.
- `APPIMAGE-READER-R016` — For a type 2 image, the reader must validate the SquashFS superblock at the payload offset.
- `APPIMAGE-READER-R017` — The reader must report the SquashFS block size, compression identifier, inode count, fragment count, format version, and the inode, directory, fragment, id, and export table offsets.
- `APPIMAGE-READER-R018` — The reader must name the SquashFS compression identifiers gzip, lzma, lzo, xz, lz4, and zstd.
- `APPIMAGE-READER-R019` — The reader must list the root directory of a type 2 payload as a set of entries with a name, an inode reference, and a type.
- `APPIMAGE-READER-R020` — The reader must read a regular file from a type 2 payload into memory by path.
- `APPIMAGE-READER-R021` — The reader must read payloads that use the uncompressed, gzip, xz, and zstd metadata and data blocks.
- `APPIMAGE-READER-R022` — The reader must report an unsupported compression identifier as an error rather than as corrupt data.
- `APPIMAGE-READER-R023` — The reader must identify the embedded desktop entry as the desktop file in the payload root directory.
- `APPIMAGE-READER-R024` — The reader must never execute, mount, or modify the input file.
- `APPIMAGE-READER-R025` — The reader must be linkable as a library and must not depend on an external AppImage library.

## Behavior

- Given a valid type 2 AppImage, inspection succeeds and the payload offset equals the offset a runtime reports for the same file.
- Given a file that is not an ELF, inspection returns a not-an-ELF error.
- Given an ELF without the AppImage magic, inspection succeeds with an unrecognized type and reports the ELF facts.
- Given a truncated file, inspection returns an error and never reads beyond the end of the file.
- Listing the payload never writes to the input file.
- A malformed SquashFS superblock produces an error, not a crash.

## Dependencies

- None.
- The payload listing depends on the SquashFS structures documented in `documents/07-appimage-format.md`.
