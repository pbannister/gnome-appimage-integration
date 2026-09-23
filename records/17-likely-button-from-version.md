# Record: which button is likely, from the version relation

## Outcome

The window now decides which button to suggest from how the downloaded AppImage's version
compares with what is already installed, and from how many launchers already exist. An older file
is visible as older before it is integrated, and integrating it anyway keeps the newer launcher.

## The Story, Implemented

| Situation | First view | After Integrate |
| --------- | ---------- | --------------- |
| same version, same file | `Close` suggested: nothing to do | — |
| same version, a different file | `Integrate` suggested | `Replace existing`, name unchanged |
| newer version | `Integrate` suggested | `Replace existing`, name unchanged |
| older version | `Close` suggested, versions shown in Status | `Add alongside`, version appended to the name |
| more than one launcher | `Integrate` suggested | `Add alongside`, version appended to the name |

"Same file" means the plan is `properly integrated`: the launcher runs this file, the file is in
the managed directory, and the record exists. Anything else that still needs doing — a repair, a
missing record, a same-version rebuild, or a newer file — suggests Integrate.

## Comparing Versions

The tool knows both versions and its readers already extract them, so the comparison lives there:
`explain --json` now carries `installed_version`, the newest version among the launchers that
represent the application, and `version_relation`, one of `newer`, `older`, `same`, or `unknown`.

`compare_versions()` splits each version into runs of digits and runs of other characters. Digit
runs compare as numbers, other runs compare case-insensitively, and a position with no token
counts as zero against a number and as a release against letters. So `1.1.3 < 1.1.10`,
`26.3.0 > 5.13.0`, `1.0 < 1.0.1`, and `1.0rc1 < 1.0`. A version that cannot be read gives
`unknown` rather than an exception, and the window treats `unknown` as "Integrate", which is what
a fresh download should do.

Verified against the real FreeCAD files: `1.1.1` against an installed `1.1.3` is `older`, `1.1.3`
against itself is `same`, and the weekly `26.3.0` against `1.1.3` is `newer`.

## Visibility of the Older Case

Status prints, before anything is clicked:

```
This AppImage is older than the installed version.
  this file:  1.1.1
  installed:  1.1.3
Close is suggested: the installed version is newer.
```

and the details block gains an `Installed version` row reading `1.1.3  (this file is older)`, so
the fact is visible without opening the log. When the owner integrates an older file anyway, the
name field is prefilled with the version appended — `Probe App 9.9.10` — and Add alongside is the
suggested choice, so the newer launcher is kept rather than displaced.

## Tests

- `tests/95-activator-ui.sh` drives the real window through the whole story. Its stub is now
  parameterised by version, installed version, relation, and launcher count, and the test asserts
  seven rows: same-version-same-file, same-version-different-file, newer, older, older-and-
  integrating, more-than-one-launcher, and newer-with-one-launcher. It also asserts the older
  wording in Status and the prefilled name in both of the cases that append the version. The
  driven-run dump prints the Name field for that.
- `tests/90-integration-conflicts.sh` asserts the relations from real synthetic AppImages:
  `1.1|1.0|newer` before the newer build is installed, `1.0|1.0|same` for the installed file, and
  `1.0|1.1|older` once the newer build owns the launcher.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed. The seven rows and the two
name prefills were also read from the installed program on a virtual display while the rules were
being built, and the version relation was read from the real FreeCAD AppImages.

## Commits

- `303fa5d` Decide the likely button from the version relation
