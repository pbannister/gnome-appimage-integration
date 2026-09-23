# Record: updating an AppImage, migrating a launcher, and two repairs

## Outcome

Four TODO items are done. `update` now replaces an installed AppImage with the file its
transport offers, after verifying the download. `migrate` adopts a launcher another tool wrote.
The dead `StartupWMClass` recovery path works, and it is what makes `migrate` keep the class the
old launcher used. The broken balenaEtcher launcher is repaired.

## Updating

`appimage-integrate update <AppImage>` is the write path; `update --check` is unchanged. The
update checks first, and acts only when the check says there is something to act on:

1. the check runs as before — GitHub API, or the `.zsync` file's `Filename:` header;
2. if the transport cannot show that the offered file is newer (a zsync file names no version),
   nothing is downloaded until `--force` is given;
3. the download goes to `<name>.part` beside the installed file, through curl, with its progress
   left on the terminal because it can be hundreds of megabytes;
4. the download is verified: the release's published `<asset>-SHA256.txt` is compared when the
   release publishes one, the downloaded file must be a readable type 2 AppImage, and its own
   `.sha256_sig` is verified when it holds a digest or a PGP signature;
5. a mismatch, or a download that is not an AppImage, refuses the update, removes the `.part`
   file and leaves the working file untouched;
6. the verified file replaces the installed one, `--backup` having first kept the old one as
   `<name>.previous`, and the new file is made executable;
7. that one launcher is re-rendered from its record — the same code `refresh` uses, factored out
   as `build_refresh_plan()` — so its desktop id, icon name, `Name=` and window class survive.

The result says which of those checks actually happened. A file with no published digest and no
signature is reported as *"no published digest or signature to check it against"*, not as verified:
that is the honest description of a download trusted on HTTPS alone.

**What is deliberately not done.** A zsync *delta* needs a zsync client; `zsync`, `zsync2` and
`appimageupdatetool` are all absent on this host, so the whole file is downloaded, and the check
prints the size first. And authenticity rests on the vendor: the FreeCAD AppImages are not signed,
so their only published digest comes from the same release over the same transport. Where a vendor
does sign, the existing `gpg --verify` path is used and the result is reported.

## Migrating

`appimage-integrate migrate <launcher>` takes a launcher file, or a desktop file ID, reads the
AppImage out of its `Exec` (falling back to `TryExec`), and installs this tool's launcher for that
file with the replace policy. The displaced launcher is backed up, so `uninstall` restores it, and
the migration is therefore reversible. It refuses when the launcher is already this tool's (and
points at `refresh`), when it names no program, and when the AppImage is not where it says.

One caveat is printed rather than hidden: if the AppImage is not already in the managed directory,
migrate moves it there, and the restored launcher will name the path it had before, because that is
exactly what the backup holds.

## The `StartupWMClass` Recovery Path

`plan()` looked for a class to borrow from `o_plan.conflicts` *before* those conflicts were
detected, so the documented order — "an explicit `--wm-class`, the embedded entry, a previous
manifest, a conflicting launcher, then a displaced launcher's backup" — had a branch that could
never fire. The adoption now runs after the conflicts are known, and the conflict detection is
repeated with the adopted class, because the class takes part in the matching that decides which
launchers conflict in the first place.

This is what makes `migrate` useful for an AppImage whose embedded entry names no class: the
launcher being replaced usually does name one, and that is the class the dock already matches on.

Moving the Exec parsing (`desktop_exec_program`) into the desktop entry reader was part of this:
`migrate` needs it, and one implementation is better than two.

## The balenaEtcher Launcher

The AppImage was not gone; it had been moved to `~/Downloads/Installers`. Re-integrating it from
there exercised the repair path: the plan reported *"repair the launcher (the AppImage is not
where it was)"*, moved the file back to `~/Applications`, re-rendered the launcher — which had
been skipped by `refresh` and so still carried only `Actions=Remove-AppImage;` — and cleared the
audit's missing-`Exec` finding. What is left in the audit for it is the shape of the AppImage
itself: no update information.

## Verification

Verified 2026-09-23 with `make test`; all twenty test scripts passed, including two new ones.

`tests/98-migrate.sh` covers a dry run that writes nothing, migration by desktop file ID, the
backup of the displaced launcher, the adoption of its window class, the reversibility of the whole
thing through `uninstall`, the no-op when the launcher is already ours, the refusal when the
AppImage is gone, and the JSON form.

`tests/99-update-apply.sh` serves an AppImage and a `.zsync` file from a local HTTP server, then
covers: a bare `update` asking for `--force` because the transport names no version, a dry run that
downloads nothing, a real replacement that changes the file's SHA-256, keeps the previous file,
makes the new one executable, re-renders the launcher and reports version 2.0.0 afterwards, a
download that is not an AppImage being refused with the working file untouched and no `.part` left
behind, and a file whose own `.sha256_sig` does not match it being refused the same way. No test
uses the outside network.

Live, on the real files: `update --dry-run` against the stale FreeCAD 1.1.1 in `~/Downloads/incoming`
names the GitHub download URL and the 820 795 896-byte asset and writes nothing; the installed
FreeCAD 1.1.3 is skipped as up to date. The 820 MB download itself was deliberately not performed.

## Commits

- `63c34c1` Update an AppImage, migrate a launcher, and repair two defects
- `f0be6fa`, `a26d08b` Record: updating, migrating, and the two repairs
