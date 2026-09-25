# Feature: MIME Association Reader

## Purpose

The project must answer "which application opens this file type, and where is that decision
recorded?" by resolving a MIME type through the MIME Applications Specification.

## Requirements

- `MIME-ASSOCIATION-READER-R001` — The reader must search `mimeapps.list` in this order: `$XDG_CONFIG_HOME`, each `$XDG_CONFIG_DIRS` entry, `$XDG_DATA_HOME/applications`, each `$XDG_DATA_DIRS` entry plus `/applications`.
- `MIME-ASSOCIATION-READER-R002` — The reader must consult the deprecated `defaults.list` only after every `mimeapps.list`.
- `MIME-ASSOCIATION-READER-R003` — The reader must read `[Default Applications]`, `[Added Associations]`, and `[Removed Associations]`.
- `MIME-ASSOCIATION-READER-R004` — The reader must treat a `[Removed Associations]` entry as removing an ID at that precedence or lower.
- `MIME-ASSOCIATION-READER-R005` — The reader must resolve a desktop file ID through the application search path, including the prefix-to-subdirectory mapping.
- `MIME-ASSOCIATION-READER-R006` — The reader must fall back to `mimeinfo.cache` in the application directories when no explicit default is recorded, because that is what the desktop uses.
- `MIME-ASSOCIATION-READER-R007` — The reader must report the winning desktop ID, its resolved path, the source file, and the group.
- `MIME-ASSOCIATION-READER-R008` — The reader must report the ordered list of files it searched.
- `MIME-ASSOCIATION-READER-R009` — The reader must skip a missing or unreadable file without error.
- `MIME-ASSOCIATION-READER-R010` — The reader must not throw.

## Behavior

- Given a default recorded in `$XDG_CONFIG_HOME/mimeapps.list`, that file is the reported source.
- Given a default whose desktop file is absent, the next ID in the same value is tried.
- Given no recorded default but a `mimeinfo.cache` association, the cache association is reported.
- The answer matches `xdg-mime query default` for the same type.

## Dependencies

- None.
