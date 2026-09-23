# Record: the Update button, and one update path

## Outcome

The activator window now has an **Update** button between Inspect and Close, and the launcher's
context menu offers **Update** — but only when the AppImage carries usable update information. The
context item does not run an update of its own: it opens the window and clicks that button, so
there is one update path with one place to watch it.

`update` itself changed to match: the offered file is downloaded **under its own name** beside the
file it replaces, every launcher and record that ran the old file is pointed at the new one, and
the old file is kept as `<name>.previous`.

## The Window

| | |
| --- | --- |
| Button order | Integrate, Run once, Inspect, **Update**, Close |
| When it appears | only when `update_usable` is true, which is what the window already reads for its "Update info" fact |
| What it does | asks `update --check --json` what the transport offers, then downloads with `update --yes --json` (adding `--force` when the transport names no version), then switches the window to the file that was installed |
| While it runs | the download is asynchronous (`GSubprocess` + `g_subprocess_communicate_utf8_async`), so the window keeps painting; the Update button is insensitive, and Status says what is being downloaded |
| When it fails | Status says so, the log is in Actions, and the window stays on the file it had |

The switch matters because the new file has its own name: after an update the window's Inspect,
Integrate and Run now refer to `FreeCAD_1.1.4-…AppImage`, not to the file that was replaced.

`--start ACTION` is what the launcher's item uses: it opens the window, performs the action, and
leaves the window open. `--activate ACTION` remains the driven form that reports the tabs and
quits, which is what the tests use. Conflating the two would have made the menu item flash a window
and exit.

## The Context Item

`Name=Update`, `Exec=<tool> handle --update <path>`, and the `Actions=` list and the action group
are written only when `understand_update_information().usable` is true. Test 96 and the unit test
assert that a file with no update information gets neither, so an item that could only report that
it cannot do anything is never offered.

## The New Shape of an Update

The offered file is the file. `FreeCAD_1.1.3-…AppImage` no longer ends up holding 1.1.4: the
transport's asset keeps its own name, which is the name the vendor gave it and the name that says
what version it holds.

1. download to `<offered name>.part` in the same directory;
2. verify it (the release's published digest, the file's own `.sha256_sig`, and that it is a
   readable type 2 AppImage) — a refusal leaves the working file untouched and removes the part;
3. move it into place and make it executable;
4. re-render every record that ran the *old* file against the new one, keeping each record's
   identity, desktop id, icon, `Name=` and window class, so a deliberate second launcher
   (OpenShot, NETGEAR) is repointed rather than left broken;
5. keep the old file as `<name>.previous`, or remove it with `--no-backup`.

Step 4 needed one new idea: while a record is being re-rendered against a *different* file, another
launcher this tool wrote for the old file is still this application, not a competitor, so
`integration_options_o::replaced_appimage_path` joins the same-file rule that
`refresh_own_launchers` already used. Without it the plan refused the update as a conflict.

`--backup` became the default and was replaced by `--no-backup`.

## Verification

Verified 2026-09-23 with `make test`; all twenty test scripts passed.

`tests/95-activator-ui.sh` covers the button row with and without update information, that
`--activate update` asks the transport and then downloads (with `--force` only when the transport
names no version), that the window switches to the installed file and reports it, that a refused
update is reported and changes nothing, and — under Xvfb — that `--start update` performs the
action and leaves the window open.

`tests/99-update-apply.sh` covers two launchers for one file, the default keep-as-`.previous`, the
retargeting of both launchers and both records, `--no-backup`, the dry run naming the file it would
download, the refusal of a download that is not an AppImage, the refusal of a file whose own
signature does not match it, and the JSON. It runs against a local HTTP server, so no test uses the
outside network.

## A State Change to Confirm

While checking the live launchers I found that `org.freecad.FreeCAD.desktop` now runs **FreeCAD
1.1.1** in `~/Applications`, with 1.1.3 beside it unrecorded. The displaced 1.1.3 launcher is
`backup/org.freecad.FreeCAD-8.desktop`, written at 15:10, and the 1.1.1 record was re-created then
with `install --replace`. No test writes outside its sandbox and no command I ran during that
window installs an AppImage, so I could not attribute it; it looks like a replace that was chosen
deliberately, from the window or the context menu. It is recorded in `TODO.md` to confirm rather
than silently reverted.

## Commits

- `d839ff5` Add an Update button, and make one update path for window and menu
- `b67c37c`, `3ce9b25` Record: the Update button and the one update path
