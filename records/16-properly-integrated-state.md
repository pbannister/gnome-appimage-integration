# Record: saying "properly integrated" instead of implying work remains

## Outcome

After Integrate has run, the AppImage is in its managed directory and recorded, and the window now
says so: the `Integrate will` row reads `properly integrated`, and the Status tab says
"This AppImage is properly integrated. Nothing more to do: the launcher and its record already
match it." The offer to replace or add alongside is gone from that state.

## The Mode

Re-integrating a complete AppImage reported `update the launcher in place`, which is true of the
work a run would do (rewrite the same bytes) but false about the state: nothing is missing. The
plan now separates the two cases:

| Condition | Mode |
| --------- | ---- |
| launcher runs this file, the file is in the managed directory, the record exists | `properly integrated` |
| launcher runs this file, but the file is not in the managed directory | `update the launcher in place` |
| launcher runs the file by identifier, at another path | `repair the launcher (the AppImage is not where it was)` |

The second case is what a custom `--install-dir` history leaves behind — the launcher is right and
the AppImage is elsewhere — so its note now says the run also places the AppImage in the managed
directory, rather than claiming to update something in place.

The record check turned out to be implied rather than separate: the upgrade classification already
requires a record that owns the launcher, so the code asks only whether the AppImage is at the
path its launcher runs it from.

## The Window

`compose_status()` reads the mode. For `properly integrated` it prints the two sentences above and
skips the "This application is already installed. Integrate will show details, then offer to
replace or add alongside." notice, so a correct integration is not invited to be replaced. If the
owner clicks Integrate anyway, the choice is still offered, with wording that matches the state:
"Replace existing rewrites that launcher and its record in place; nothing is missing."

The `Integrate will` details row needs no change: it already prints the tool's mode, so it now
reads `properly integrated` by itself.

## Tests

- `tests/90-integration-conflicts.sh`: re-integrating the managed copy expects `properly
  integrated` and the `already integrated:` note; a plan with `--install-dir` pointing elsewhere
  expects `update the launcher in place`.
- `tests/95-activator-ui.sh`: the stub's mode is now chosen by `STUB_MODE` (the assignment on the
  command line, because a plain shell variable would not reach the child), and a driven run with
  `properly integrated` asserts the Status wording and that the replace notice is absent.

## On This Host

No AppImage is currently complete, and the modes say why: `~/Applications` was moved to
`~/Downloads/Applications`, so OpenShot, OrcaSlicer, and Cura 5.13 are `repair the launcher (the
AppImage is not where it was)`, while FreeCAD 1.1.3 and Cura 5.10.1 are `another launcher already
represents this application`. Integrating any of them returns the state to `properly integrated`,
which is the state the owner asked to be able to recognise.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed, and the installed tool was
run over every AppImage in `~/Downloads/Applications` to read the modes above.

## Commits

- `a021f7b` Report a complete integration as properly integrated
