# Record: resolving and checking the update information

## Outcome

The `.upd_info` field is no longer just printed. `appimage-inspect --update-url` resolves it
offline into the URL and file pattern it names; `appimage-integrate update --check [--all]` asks
the transport what it has and compares the answer with the installed version; `audit` reports a
recorded AppImage whose update information is missing or unusable, and `audit --check` reports
which ones have an update waiting; and every launcher now carries a **Check for updates** context
action. Nothing downloads or installs: checking is a read.

Fixing this also uncovered a real defect in the version comparison, which is fixed here.

## The Field

`.upd_info` holds a transport name followed by `|`-separated fields. The specification defines
four transports:

| Transport | Fields | State |
| --------- | ------ | ----- |
| `zsync` | the URL of the `.zsync` file | usable; the URL must be HTTP or HTTPS |
| `gh-releases-zsync` | user, repository, release, `*.zsync` file name pattern | usable |
| `pling-v1-zsync` | Pling product id, `*.AppImage` file pattern | recognised, cannot be checked: Pling does not serve the zsync file, so it needs Pling's own API |
| `bintray-zsync` | legacy | dead: Bintray was shut down in 2021 |

The GitHub release field takes `latest`, `latest-pre` and `latest-all` as special values, and any
other value is a tag. GitHub has no latest-prerelease endpoint, so the two prerelease forms resolve
to the release list and the caller takes the first entry that fits, newest first.

Anything else is not a transport. On this host the two Cura AppImages carry the literal string
`guess`, which is an `appimagetool` command-line option that some tool embedded as if it were
update information; it is now named as unknown rather than silently ignored.

## What Was Built

| File | Role |
| ---- | ---- |
| `sources/appimage/appimage_update_information.{h,cpp}` | understand the value: transport, fields, usability, the request URL, the asset patterns, and a sentence for people; plus the `*`/`?` pattern match the specification's file names need |
| `sources/version/version_compare.{h,cpp}` | the version comparison, moved out of the integrator so the update check can use the same one |
| `sources/tools/appimage_inspect.cpp` | `--update-url`, and the resolved fields in `--json` |
| `sources/tools/appimage_integrate.cpp` | `update --check [--all] [--json] [--notify]`, `audit --check`, and the `update_*` fields in `explain --json` |
| `sources/integration/appimage_integrator.cpp` | the update-information audit findings, the richer `explain` text, the public `appimage_version()`, and the third context action in the launcher template |
| `sources/tools/appimage_activator_ui.cpp` | the Discovered fact now says what the value means, or why nothing can be done with it |
| `tests/version_compare_test.cpp`, `tests/appimage_update_information_test.cpp`, `tests/15-update-information.sh` | unit tests for both, wired into `make test` |
| `tests/97-update-check.sh` | the whole path, including a local HTTP server |

## Decisions

**Check, never follow.** The field is the file's own claim about where its successors live.
Reading it is free; acting on it means asking a third party for a binary. So the resolution and
the check are separate commands, the check says when it cannot tell, and downloading is left to a
later episode with an explicit trust decision. That decision is not obvious here: the FreeCAD
AppImages carry no PGP signature (their `.sha256_sig` is empty padding), so the only published
digest comes from the same release over the same transport.

**A zsync file carries no version.** Its header names the file it updates (`Filename:`), and
comparing that file name with a version string would be nonsense, so the zsync transport answers
only whether the offered file name is the installed one: *up to date: the transport offers the file
that is installed*, or *a different file is offered: … (the transport names no version, so this
tool cannot tell whether it is newer)*. Only `gh-releases-zsync` produces a version relation, and
there a release tag's leading `v` is stripped before comparison because that is how people tag
releases.

**The check does not depend on integration.** Reading the file is what a check is about, so it
uses the container reader, not `plan()`: an AppImage that conflicts with an installed launcher, or
that is not integrated at all, is still checkable. The first version used `plan()` and reported a
launcher conflict as the result of the check, which is how the stale `Downloads` copy of FreeCAD
1.1.1 came back as "cannot check" while carrying perfect update information.

**Exit non-zero when a check could not be made**, and zero when every requested check completed,
whether or not an update exists.

## The Defect It Found

Moving the version comparison into `sources/version/` and writing a unit test for it exposed an
inverted sign in the missing-token branch: the side with no token is compared as zero against a
number, and the answer was not flipped when that side was the right-hand one. The effect was that
`1.0` sorted *newer* than `1.0.1`, and an empty version sorted newest of all — the opposite of
what the function's own comment promised. `compare_versions("1.0", "1.0.1")` returned `1`; it now
returns `-1`. The existing tests only compared equal-length versions (`1.1` against `1.0`), so
nothing had caught it.

## Live Verification

Run on this host, with the real AppImages:

| File | Result |
| ---- | ------ |
| `~/Applications/FreeCAD_1.1.3-…` | up to date (1.1.3), asset `FreeCAD_1.1.3-Linux-x86_64-py311.AppImage` (820 795 896 bytes), zsync 1 603 374 bytes |
| `~/Downloads/incoming/FreeCAD_1.1.1-…` | an update is available: 1.1.3 — the stale copy that keeps showing up as a conflict in `list` |
| `~/Applications/UltiMaker-Cura-5.13.0-…` | cannot check: `"guess"` is not a transport the AppImage specification defines |
| the other recorded AppImages | cannot check: no update information in the file |

`update --check --all` deduplicated the records to six distinct files (three NETGEAR launchers
share one AppImage) and reported `checked 1 of 6, could not check 5, updates available 0`.
`audit` now reports the missing and unusable values, `audit --check` adds `up to date: 1.1.3` for
FreeCAD. `refresh` rewrote all eight launchers, each now carrying
`Actions=AppImage-Activator;Update-AppImage;Remove-AppImage;` with a
`[Desktop Action Update-AppImage]` group running
`appimage-integrate update --check --notify <AppImage>`, and `make install` placed the matching
tool in `~/.local/bin` so the action works.

## Verification

Verified 2026-09-23 with `make test`; all test scripts passed. The two new unit tests cover the
transports (each field count, the special release values, dead and unknown transports), the URL
resolution, the pattern match, and the version ordering including the fixed case. The new
`tests/97-update-check.sh` builds AppImages whose `.upd_info` is a real ELF section, resolves them,
runs `explain --json` through to the activator's fields, refuses an unknown transport, refuses a
bare `update`, and then serves a `.zsync` file from a local HTTP server on 127.0.0.1 to check both
answers — a different file offered, and the installed file offered — plus the launcher's new action
group and the audit findings. No outside network is used by any test.

## Follow-Up

- Actually updating is still a `TODO.md` item, with the two decisions above recorded there.
- `pling-v1-zsync` is recognised but cannot be checked; no AppImage on this host uses it.

## Commits

- `0487dca` Resolve and check the AppImage update information
