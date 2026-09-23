# Record: a second handler window, and existing integrations that were being taken over

## Outcome

Two reports were fixed. Opening the handler on a second AppImage now opens a window for that
AppImage, and an existing integration is detected and offered as a choice instead of being
silently replaced.

## A Second Handler Window Showed the First File

The handler is a `Gtk.Application`, and GTK's default is single-instance: the second launch
forwarded its file to the first process, whose window still showed the first AppImage.

Fix: `Gio.ApplicationFlags.NON_UNIQUE` (line 607), so each launch is its own application
instance with its own window and its own file.
`tests/95-handler-ui.sh` asserts the flag is present.

## Existing Integrations Were Detected but Thrown Away

`explain --json` already reported conflicts, but the handler's `conflicts()` filtered out every
entry whose `upgrade` flag was set, so a launcher this tool had written looked like nothing at
all and Integrate went straight to install. Install then rewrote that launcher in place.

Fixes:

- `conflicts()` returns every existing launcher, upgrades included, and Integrate prompts for
  any of them.
- The prompt says "This application is already installed", lists each launcher through
  `describe_conflict` (id, name, origin, version, target and whether it exists, icon, class,
  file), and offers Back, Add alongside, and Replace existing.
- Upgrade-aware wording: a lone upgrade is described as "upgrades that launcher in place",
  anything else as backing the launchers up and installing in their place.

## A Different AppImage Was Still a Silent Takeover

`detect_application_conflicts` treated *any* launcher this tool owned at the target identifier
as an in-place upgrade. Two different AppImages of one application share the embedded desktop
file name, so the second silently took the identifier from the first — which is exactly what
the FreeCAD pair on this host did, leaving two records claiming one launcher.

Fixes, in `sources/integration/appimage_integrator.cpp`:

- In-place upgrade now requires that the launcher already runs *this very file*
  (`same_file_path`, which canonicalises so a symlink or another spelling still matches).
- Anything else is a conflict, named
  `this tool (a different AppImage for the same identifier)`, and refused until `--replace` or
  `--add` is chosen.
- `--add` now suffixes the record and the icon as well as the launcher:
  `<id>-N.desktop`, record `<identifier>-N`, icon `<icon-name>-N`. Without that, the new record
  was written to the same manifest path and destroyed the record of the launcher it was told to
  keep, which made the kept launcher look foreign.
- Manifest retirement is narrowed to the launchers that really lose their place: the one being
  replaced, or the one at the identifier being upgraded in place. `--add` no longer touches any
  existing record.
- The origin of a launcher that carries `X-AppImage-Identifier` but has no record now checks
  `X-Integrated-By` first. Both this tool and AppImageLauncher write the identifier key, so
  provenance decides: `this tool (launcher with no record)` versus `AppImageLauncher`.
- `audit` gained the same distinction, plus a warning when a record disagrees with the AppImage
  its launcher actually runs, which is the state a silent takeover leaves behind.
- Upgrade conflicts now carry their version, icon, window class, and whether the target exists,
  so the handler's list is complete.

## Tests

- `tests/90-integration-conflicts.sh`: `--add` on an upgrade-only conflict makes a second
  launcher and a second record; both keep their provenance (`this tool` and
  `this tool (upgrade)`, with the upgrade reporting version `9.9.10` and a present target); a
  third AppImage wanting the same identifier makes `plan` fail and name the takeover, and
  `--add` then installs it as `org.example.Probe-3.desktop`.
- `tests/70-integration-sandbox.sh`: a record edited to disagree with its launcher is reported
  by `audit`.
- `tests/95-handler-ui.sh`: the handler is multi-instance and reports existing integrations.

## State on This Host After the Fix

`explain` now reads the real FreeCAD pair correctly: the weekly AppImage is told that
`org.freecad.FreeCAD.desktop` runs `FreeCAD_1.1.3-…` (`this tool (a different AppImage for the
same identifier)`, version `1.1.3`), so integrating it is an explicit choice. The stable 1.1.3
is still an in-place upgrade of its own file.

`audit` no longer mislabels this tool's own launchers as AppImageLauncher's, and it reports the
one unrelated leftover: an AppImageLauncher-era icon in a `0x0` size directory.

## Verification

Verified 2026-09-23 with `make test`; all fifteen test scripts passed. The updated tool and
handler were installed to `~/.local/bin`, and the conflict output above was read from the real
`~/.local/share/applications` and `~/Applications`.

## Commits

- `6474bbf` Never take over an identifier that another AppImage already owns
