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

When this work was done, no AppImage was complete and the modes said why: `~/Applications` had
been moved to `~/Downloads/Applications`, so OpenShot, OrcaSlicer, and Cura 5.13 were `repair the
launcher (the AppImage is not where it was)`, while FreeCAD 1.1.3 and Cura 5.10.1 were `another
launcher already represents this application`. The owner then moved the directory back, and
re-reading every AppImage now reports `properly integrated` for FreeCAD 1.1.3, NETGEAR Discovery
Tool, OpenShot, OrcaSlicer, and Cura 5.13 — the state this episode added.

## Where the Window Lands

Integrate wrote its log to Actions and left Actions showing, so the owner saw what was done but
not what the state had become. Each action now raises the page that answers it: Integrate the
Status page, which is where `properly integrated` appears; Inspect the Discovered page, with the
appended report; and Run once or Run now the Actions page, where the output of the run belongs.
A driven run prints which page is showing, and the test asserts all three.

## Button Order and the Suggested Hint

The first view offered `Run once, Integrate, Inspect, Close`; it is now `Integrate, Run once,
Inspect, Close`, ordered by how likely the owner is to use each button, and Integrate carries the
GNOME HIG's
[suggested-action](https://developer.gnome.org/hig/patterns/controls/buttons.html) style, which
"highlights a button for affirmative action … to draw attention to the next step in a process".
The HIG allows one suggested button per view, so after a successful integration the row is
`Run now, Inspect, Close` with Run now suggested, and the conflict choice is
`Replace existing, Add alongside, Back` with Replace existing suggested.

No button is bound to Return. The HIG permits leaving the default unset, and a keystroke should
not move files; the suggestion is a visual hint.

A driven run prints the real widget row with `*` on the suggested button, and the test asserts
every row, so the order and the style are checked against the widgets rather than the source text.

## Emphasising the Two Fields That Decide

The Discovered log became long enough that the two lines the owner acts on were lost in it. The
`This run` and `Error` fields are now bold, in the facts the window composes and in the report
Inspect appends, and nothing else is: the match is by case-insensitive prefix, so `This run …` and
`this run: …` are both caught, as are `Error …` and `install preview unavailable: …`. The tag is
re-applied whenever the log changes, because replacing text drops tag applications.

A driven run prints the tagged lines under `=== highlighted ===`, and the test asserts that both
fields appear there and that a plain field such as `Detection` does not.

## The Conflict Choice, Reordered

`Back, Add alongside, Replace existing` became `Replace existing, Add alongside, Back`, with
Replace existing suggested. That is the owner's ordering rule — the most likely action first —
applied to this row as it was to the first view. The GNOME HIG's dialog guideline puts the cancel
button first, but that rule is about modal dialogs; this window is a modeless utility window whose
Status tab explains both choices before either is clicked, so the owner's rule governs here.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed, and the installed tool was
run over every AppImage in `~/Downloads/Applications` to read the modes above.

## Commits

- `a021f7b` Report a complete integration as properly integrated
- `1317732` Show the Status tab after Integrate
- `a0554d8` Order the first view's buttons by likely use, and hint Integrate
- `fa32d8b` Emphasise This run and Error, and put Replace existing first
