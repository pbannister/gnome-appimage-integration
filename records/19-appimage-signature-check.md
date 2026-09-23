# Record: checking the `.sha256_sig` section, and a 64-bit ELF defect it exposed

## Outcome

Integration now checks the AppImage signature section when it holds something to check, and the
last AppImageLauncher artefact is gone from this host. The signature work also uncovered a real
reader defect: on every 64-bit ELF, section offsets and sizes were read from the 32-bit field
positions, so they came back as zero.

## What the Section Means

The AppImage specification says a type 2 AppImage *may* embed a digital signature in `.sha256_sig`,
and that if the section exists it **must** be either empty (`0x00` padding) or a valid signature of
the SHA-256 of the AppImage *assuming that section is filled with zero padding*. The signing code
in AppImageKit confirms the mechanics: `appimagetool` computes the digest while the section is
still zeros, signs that hex digest with gpg, and writes the signature into the section.

So the digest covers the whole file with the section zeroed, and the section holds either the hex
digest or a signature over it.

## What Was Built

| File | Role |
| ---- | ---- |
| `sources/crypto/sha256.{h,cpp}` | SHA-256, streaming, with `update_zeros` for the zeroed section |
| `tests/sha256_test.cpp`, `tests/13-sha256.sh` | the FIPS 180-4 vectors, chunked hashing, zero padding, and a cross-check against `sha256sum` |
| `sources/appimage/appimage_signature.{h,cpp}` | classification, the zeroed-section digest, `gpg --verify`, and the labels |
| `sources/appimage/appimage_reader.*` | classifies the section while reading; the enums and fields live with the facts |
| `sources/integration/…`, `sources/tools/…` | the plan, `explain`, `plan`, `install`, `appimage-inspect`, `--ignore-signature`, and the JSON fields |
| `tests/92-appimage-signature.sh` | builds AppImages with a real `.sha256_sig` section and checks all of it |

Classification: absent, zero padding, a 64-digit hex digest (optionally `sha256:`-prefixed), an
ASCII-armoured PGP signature, or unrecognised. A hex digest is compared with the digest of the file;
a signature is checked by writing the computed hex digest to a temporary file and running
`gpg --verify <signature> <digest>`, which verifies both authenticity and that the file has not
changed. A missing gpg, or a signature whose key is not in the keyring, is reported as *not
verified*, never as a mismatch.

`verify_appimage_signature` hashes the whole file, so it is called where the cost is justified:
`plan()` when the section holds a digest or signature (so `explain`, and therefore the activator,
see it), `appimage-inspect` for a deliberate inspection, and the human `explain` report. An
unsigned file costs nothing.

A definite mismatch makes `install` refuse, naming `--ignore-signature`, which proceeds and records
"the signature mismatch is ignored on request" in the plan. Status in the activator shows the stored
and computed digests and the refusal, so the owner sees why before anything is written.

## The 64-bit ELF Defect

`read_section_header()` compared `ELF_CLASS_64 == i_elf_class`, but the reader stores `elf_class` as
a bit width (64 or 32) while `ELF_CLASS_64` is the raw `e_ident` class (2). The comparison was
therefore never true, and every section's offset and size were read from the 32-bit field positions
— zero for a 64-bit file. The name offset, which sits first in both layouts, was read correctly,
which is why section *names* were found and only their contents were wrong.

Consequences until now: `.upd_info` and `.sha256_sig` contents were never read (so update
information was empty and the signature always looked empty), and the ELF size estimate fell back to
the section table end. Nothing caught it because no synthetic AppImage carried either section, and
the real AppImages here have no `.upd_info` and no signature. The new signature test caught it
immediately: the section was visible to `readelf` and invisible to the reader.

## The Artefact

`~/.local/share/icons/hicolor/0x0/` held one AppImageLauncher-era icon,
`appimagekit_5fde0018…_nsdt-app.png`, in a size directory that no theme lookup reads. No launcher
referenced it, and `audit` warned about the directory on every run. It is removed, and `audit` now
has no `0x0` finding. This was the last AppImageLauncher leftover.

## Verification

Verified 2026-09-23 with `make test`; all eighteen test scripts passed, including the new SHA-256
and signature tests. The signature test builds its own ELFs with `objcopy --add-section`, writes the
digest of the assembled file into the section, and asserts: zero padding is recognised, a correct
digest verifies in `explain --json`, `plan`, `explain`, `appimage-inspect`, and `install` succeeds, a
payload swapped after signing reports a mismatch, `install` refuses it and names the override, and
`--ignore-signature` integrates it and says so in the plan.

## Commits

- `1b6e31c` Check the `.sha256_sig` section, and read 64-bit section headers correctly
- `TODO` edit by the owner took the two items off the open list
