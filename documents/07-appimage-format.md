# The AppImage File Format

This document records the AppImage format as specified by the AppImage project,
the payload layout the reader depends on, and the byte-level detection rules.

Live-state facts in this document were verified on 2026-09-21 against the sources listed below.

## Sources

- AppImage Specification, working draft: <https://github.com/AppImage/AppImageSpec/blob/master/draft.md>
- AppImage documentation, AppDir reference: <https://docs.appimage.org/reference/appdir.html>
- AppImage runtime (reference implementation): <https://github.com/AppImage/AppImageKit/blob/master/src/runtime.c>
- libappimage, `ElfFile.cpp` (payload offset algorithm): <https://github.com/AppImageCommunity/libappimage/blob/master/src/libappimage/utils/ElfFile.cpp>
- libappimage, `type2.c` (payload access): <https://github.com/AppImageCommunity/libappimage/blob/master/src/libappimage/type2.c>
- SquashFS on-disk structures: <https://github.com/plougher/squashfs-tools/blob/master/squashfs-tools/squashfs_fs.h>
- SquashFS kernel documentation: <https://docs.kernel.org/filesystems/squashfs.html>

## Definitions

- An **AppImage** is a single-file executable that packages an application and its dependencies.
- An **AppDir** is a directory tree that holds the application, its metadata, and its icons.
- The **runtime** is the ELF program prepended to the AppImage that mounts the payload and executes `AppRun`.
- The **payload** is the filesystem image appended to the runtime.

## Image Formats

The specification defines three image types, distinguished by a three-byte magic value at offset 8 of the file.

| Type | Magic at offset 8 | Filesystem | Metadata carrier |
| ---- | ----------------- | ---------- | ---------------- |
| 0 | none | unspecified | reserved for non-compliant files |
| 1 | `0x41 0x49 0x01` (`AI\x01`) | ISO 9660 with Rock Ridge | ISO 9660 Volume Descriptor at offset 33651 |
| 2 | `0x41 0x49 0x02` (`AI\x02`) | SquashFS appended to the ELF | ELF sections `.upd_info` and `.sha256_sig` |

Both type 1 and type 2 are valid ELF executables.
The magic occupies bytes 8 to 10, which are part of the ELF `e_ident` padding field.

### Type 1 rules

- It must be an ISO 9660 file that uses Rock Ridge extensions and may use Joliet extensions.
- It should use zisofs compression.
- It must be a valid ELF executable.
- It may embed update information in the ISO 9660 Volume Descriptor field at offset 33651.

### Type 2 rules

- It must be a valid ELF executable.
- It must have a filesystem appended to it that the ELF part can mount.
- It may embed update information in the ELF section `.upd_info`.
- It may embed a digital signature in the ELF section `.sha256_sig`.
- If `.sha256_sig` exists, it must either be empty (filled with `0x00` padding) or contain a valid signature of the SHA-256 of the AppImage with that section zeroed.
- It must contain the magic `0x414902` at offset 8.

### Payload offset for type 2

The runtime computes the payload offset as the size of the ELF part, using only the ELF header and the last section header.

```
sht_end          = e_shoff + (e_shentsize * e_shnum)
last_section_end = sh_offset + sh_size          # of the section header at index e_shnum - 1
elf_size         = max(sht_end, last_section_end)
payload_offset   = elf_size
```

The algorithm selects the 32-bit or 64-bit ELF header from `e_ident[EI_CLASS]`,
converts multi-byte fields from the file byte order selected by `e_ident[EI_DATA]`,
and reads only the final section header rather than the whole table.
The payload size is the file size minus `payload_offset`.

## Contents of the Image

The payload filesystem is an AppDir and carries a fixed set of members.

| Path | Rule | Purpose |
| ---- | ---- | ------- |
| `AppRun` | must exist and be executable | entry point the runtime executes |
| `$APPNAME.desktop` | should exist, exactly one, at the root | desktop entry for the payload |
| `.DirIcon` | must exist | icon per the AppDir specification, should be a 256x256 PNG |
| `$APPICON.svg`, `$APPICON.svgz`, or `$APPICON.png` | may exist at the root | fallback icon named by the `Icon=` key |
| `usr/share/icons/hicolor/...` | should exist | themed icons named by the `Icon=` key |
| `usr/share/metainfo/$ID.appdata.xml` | should exist | AppStream metadata |

- `$APPNAME` is the name of the payload application.
- `$APPICON` is the icon identifier set in the `Icon=` key of `$APPNAME.desktop`.
- Icons below `usr/share/icons/hicolor` take preference over a root icon file when both match the identifier.
- `AppRun` may be an ELF binary or an interpreted script.
- `AppRun` should pass its arguments and environment to the payload application.

## Embedded Metadata

### Update information

Type 2 AppImages store update information in the ELF section `.upd_info`.
The content is an ASCII string; a known transport mechanism is required, otherwise it should be empty or ignored.

| Transport | Form |
| --------- | ---- |
| `zsync` | `zsync\|https://server.domain/path/Application-latest-x86_64.AppImage.zsync` |
| `gh-releases-zsync` | `gh-releases-zsync\|user\|repo\|tag\|filename.zsync` |
| `pling-v1-zsync` | `pling-v1-zsync\|product-id\|pattern` |

The GitHub release tag accepts the special values `latest`, `latest-pre`, and `latest-all`.

### Signature

Type 2 AppImages may store a signature in the ELF section `.sha256_sig`.
The signature covers the SHA-256 digest of the AppImage with the `.sha256_sig` section replaced by zero padding.

## Payload Filesystem: SquashFS

The type 2 payload is a SquashFS image. Its superblock begins at `payload_offset`.

| Field | Width | Notes |
| ----- | ----- | ----- |
| `s_magic` | 4 | `0x73717368`, the bytes `hsqs` in little-endian order |
| `inodes` | 4 | inode count |
| `mkfs_time` | 4 | creation timestamp |
| `block_size` | 4 | data block size, commonly 131072 |
| `fragments` | 4 | fragment count |
| `compression` | 2 | compression identifier |
| `block_log` | 2 | log2 of `block_size` |
| `flags` | 2 | filesystem flags |
| `no_ids` | 2 | uid/gid table entries |
| `s_major` / `s_minor` | 2 / 2 | format version |
| `root_inode` | 8 | packed root inode reference |
| `bytes_used` | 8 | filesystem size in bytes |
| `id_table_start` | 8 | uid/gid lookup table start |
| `xattr_id_table_start` | 8 | extended attribute table start |
| `inode_table_start` | 8 | inode table start |
| `directory_table_start` | 8 | directory table start |
| `fragment_table_start` | 8 | fragment table start |
| `lookup_table_start` | 8 | export (NFS) table start |

Compression identifiers in the `compression` field are:

| Value | Compression |
| ----- | ----------- |
| 1 | gzip (zlib) |
| 2 | lzma |
| 3 | lzo |
| 4 | xz |
| 5 | lz4 |
| 6 | zstd |

Additional constants used by a reader:

- Metadata blocks are at most `SQUASHFS_METADATA_SIZE` = 8192 bytes.
- Data blocks are at most `SQUASHFS_FILE_MAX_SIZE` = 1048576 bytes.
- A missing value is `SQUASHFS_INVALID` (`0xffffffffffff`) or `SQUASHFS_INVALID_BLK` (`-1`).
- Inode types include `DIR=1`, `FILE=2`, `SYMLINK=3`, `LDIR=8`, `LREG=9`, and `LSYMLINK=10`.
- `root_inode` is a packed reference: the upper bits index a metadata block and the low 16 bits give the offset within it.

The reference implementations read the payload through libsquashfuse rather than a private SquashFS implementation.

## Desktop Integration

The software inside an AppImage may integrate with the host desktop, but it should ask the user first.

Desktop integration should be skipped when any of these hold:

- A file `$XDG_DATA_HOME/appimagekit/no_desktopintegration` exists.
- A file `/usr/share/appimagekit/no_desktopintegration` exists.
- A file `/etc/appimagekit/no_desktopintegration` exists.
- A process named `appimaged` is running.
- The environment variable `$DESKTOPINTEGRATION` is non-empty.

A static runtime must support `TARGET_APPIMAGE`; when it names an existing path, the runtime uses that filesystem image instead of its own appended one.

## Consequences for the Reader

The AppImage reader in this project parses the container natively and does not depend on libappimage.

- Detect the file type from the ELF magic and the AppImage magic at offset 8.
- Report ELF class, byte order, machine, entry point, and the payload offset and size.
- Read the section header string table to expose `.upd_info` and `.sha256_sig`.
- Parse the SquashFS superblock at the payload offset and report its compression and table layout.
- Traverse the SquashFS payload enough to list the root directory and read the embedded `*.desktop` file and `.DirIcon`.

File name conventions are recommendations, not requirements; the reader must not depend on the `.AppImage` extension.

## Verification

- Spec facts: verified 2026-09-21 against the AppImage Specification working draft and the libappimage sources.
- Payload offset algorithm: verified 2026-09-21 against `ElfFile.cpp` and `runtime.c`.
- SquashFS structures: verified 2026-09-21 against `squashfs_fs.h` from squashfs-tools 4.6.1.
- Tooling on this host: `mksquashfs` and `unsquashfs` version 4.6.1 (2023-03-25).
</content>
