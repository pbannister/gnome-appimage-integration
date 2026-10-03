# Record: the root desktop entry that was a symlink

## Outcome

Integrating Audacity 4.0.0 completes instead of failing with *"the AppImage has no root desktop
entry"*.  The SquashFS reader follows symbolic links in a payload the way the kernel does, so a root
desktop entry that is a link into the AppImage's own `share/applications` is read, a path through a
directory link resolves, and the payload's MIME package is found again on AppImages that use the
`usr -> .` layout.

## What Happened

The reported failure was `appimage-integrate` on `audacity-linux-4.0.0-x86_64.AppImage`:

```
State:     unknown
Launcher:  (none yet)
the AppImage has no root desktop entry
```

The AppImage is not malformed.  Its root holds a **link**:

```
lrwxrwxrwx  org.audacityteam.Audacity4portable.desktop -> share/applications/org.audacityteam.Audacity4portable.desktop
lrwxrwxrwx  .DirIcon -> audacity4portable.png
lrwxrwxrwx  audacity4portable.png -> share/icons/hicolor/64x64/apps/audacity4portable.png
lrwxrwxrwx  usr -> .
```

Linking the root desktop name into `share/applications` is an ordinary AppImage shape, and the
AppImage runtime resolves it.  The reader did not:

- `list_root_files_with_extension` accepted only `regular_file`, so the root desktop entry was not
  listed at all — the exact error above;
- `read_file` refused anything that was not a regular inode, so a link could not be read even if it
  had been listed;
- `resolve_path` required every intermediate component to be a directory, so `usr -> .` made
  `/usr/share/...` unreachable.  That silently cost the payload's own MIME definition
  (`audit` and `plan` reported no MIME packages for this AppImage).

`.DirIcon` being a link had the same effect on the icon, though the root fallback found `aup4.svg`,
so the icon was not the visible symptom.

## The Fix

| Change | Where |
| ------ | ----- |
| `is_symlink_inode` | `sources/appimage/squashfs_reader.cpp` |
| `split_path_components`, shared by both resolvers | `sources/appimage/squashfs_reader.cpp` |
| `resolve_path_following_symlinks`: follows a link in any position, resolves a relative target against the directory holding the link, steps back out of `..` through a stack of the directories walked, and bounds a chain at 16 | `sources/appimage/squashfs_reader.cpp` |
| `read_file` and `list_directory` use it | `sources/appimage/squashfs_reader.cpp` |
| The root desktop listing takes real files first, then links that resolve to a regular file, skipping an inode already listed | `sources/appimage/squashfs_reader.cpp` |
| Fixture gains the Audacity shape, a directory link, and a read through a link | `tests/40-squashfs-payload-reader.sh` |
| Assertions for both, and for the no-duplicate rule | `tests/squashfs_reader_test.cpp` |

## Decisions

- **`stat` does not follow links.**  It reports the node the path names, so a link stays a link; the
  existing test asserting `stat("/link.desktop").type == symlink` still holds.  `read_file` and
  `list_directory` are the operations that mean "the file (or directory) this path names", and those
  follow.
- **A link is only a last resort for the root desktop entry.**  Real files are taken first, and a
  resolved inode is taken once.  A link that merely repeats a root file — the reader test's
  `link.desktop -> test.desktop` — must not read as a second root entry and make the integrator
  report a conflict with itself.  A link that is the only root entry is exactly the Audacity case and
  is accepted.
- **The directory-link fix is part of the same repair, not a separate one.**  It was found while
  checking the first: without it the same AppImage loses its MIME definition, which is a silent
  wrong answer rather than a loud failure.

## Verification

verified 2026-10-02:

- `tests/40-squashfs-payload-reader.sh` passes for gzip, xz, zstd, and uncompressed blocks.  Its
  fixture is now the Audacity shape: a root `.desktop` that is only a link, plus `sharealias -> usr/share`.
- The reader test asserts a read through a link equals the target's bytes, that a path through a
  directory link lists, that `stat` still reports a link as a link, that the root desktop listing is
  `test.desktop` then `org.example.Symlinked.desktop`, and that `link.desktop` is not listed twice.
- `make test` passes every script.
- Against the real `audacity-linux-4.0.0-x86_64.AppImage`: `appimage-inspect` reports
  `embedded-desktop: /org.audacityteam.Audacity4portable.desktop`; `plan` completes with eight icon
  sizes, `mime-packages: /usr/share/mime/packages/audacity4portable.xml`, desktop id
  `org.audacityteam.Audacity4portable.desktop`, and `startup-wm-class: Audacity 4` (the embedded
  value, and the new warning fires on it as designed); a sandboxed `install` writes a launcher with
  the right `Exec`, `Icon`, `MimeType`, and `StartupWMClass`.
- A read-only sweep of all fifteen AppImages on this machine: every one still resolves its embedded
  desktop entry and plans without error, so the reader change altered no working case.

## Commits

- `58013bb` Read a root desktop entry that is a symlink

## If It Happens Again

"no root desktop entry" is a claim about the payload, so check it before believing it:

```
unsquashfs -ll -offset <payload-offset> <AppImage> | grep -E '\.desktop|^l'
appimage-inspect <AppImage>            # the payload root, with each entry's node type
```

The offset is the `payload-offset` that `appimage-inspect` prints.  A root entry that shows as
`symlink` is normal; if the tool ever again says there is none, the reader is the suspect, not the
AppImage.
