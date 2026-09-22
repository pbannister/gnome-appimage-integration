# Task: Implement the MIME Association Reader feature

## TASK-DESCRIPTION
* Create `sources/desktop/mime_association_reader.h`.
* Create `sources/desktop/mime_association_reader.cpp`.
* Create the portable test `tests/mime_association_reader_test.cpp`.
* Create the test runner `tests/22-mime-association-reader.sh`.
* Implement the requirements in `prompts/features/08-mime-association-reader.md`.
* Run `make test` from the repository root.

## TASK-OUTPUT
* Report the created files and the result of `make test`.

## TASK-CONTEXT
* Feature requirements: `prompts/features/08-mime-association-reader.md`.
* MIME search order: `documents/12-desktop-loading-and-provenance.md` section 4.
* The `mimeinfo.cache` fallback must match what `xdg-mime query default` reports.

## TASK-FILES
- `sources/desktop/mime_association_reader.h` — new
- `sources/desktop/mime_association_reader.cpp` — new
- `tests/mime_association_reader_test.cpp` — new
- `tests/22-mime-association-reader.sh` — new
