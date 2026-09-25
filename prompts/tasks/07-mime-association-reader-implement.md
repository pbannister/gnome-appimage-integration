# Task: Implement the MIME Association Reader feature

## TASK-DESCRIPTION
- Create: `sources/desktop/mime_association_reader.h`
- Create: `sources/desktop/mime_association_reader.cpp`
- Create: `tests/mime_association_reader_test.cpp`
- Create: `tests/22-mime-association-reader.sh`
The reader resolves which desktop file opens a MIME type and reports where that answer came from, and implements the requirements in `prompts/features/08-mime-association-reader.md`.
The fallback to `mimeinfo.cache` must agree with what `xdg-mime query default` reports for the same type.

## TASK-OUTPUT
Produce these complete files in this order, then the `VERIFICATION:` line:
1. `sources/desktop/mime_association_reader.h`
2. `sources/desktop/mime_association_reader.cpp`
3. `tests/mime_association_reader_test.cpp`
4. `tests/22-mime-association-reader.sh`

## TASK-CONTEXT
<note>
The MIME search order is described in `documents/12-desktop-loading-and-provenance.md` section 4. A missing or unreadable file is skipped without error, and the reader never throws.
</note>

## TASK-FILES

| Operation | Path |
|---|---|
| create | `sources/desktop/mime_association_reader.h` |
| create | `sources/desktop/mime_association_reader.cpp` |
| create | `tests/mime_association_reader_test.cpp` |
| create | `tests/22-mime-association-reader.sh` |

## TASK-VERIFY
- Run: `make test` from the repository root.
- Expected: exit status 0, and the reported default matches `xdg-mime query default` when `xdg-mime` is installed.

## TASK-FEATURES
- `prompts/features/08-mime-association-reader.md`

## TASK-ACCEPTANCE
- `MIME-ASSOCIATION-READER-R001`
- `MIME-ASSOCIATION-READER-R002`
- `MIME-ASSOCIATION-READER-R004`
- `MIME-ASSOCIATION-READER-R006`
- `MIME-ASSOCIATION-READER-R007`
- `MIME-ASSOCIATION-READER-R010`

OUTPUT: the four complete files in the order listed, ending with the `VERIFICATION:` line.
