# Record: Status, Discovered, and Actions, and a mode that is not a failure

## Outcome

The activator's single text area is now a tabbed panel of three read-only logs, so the current
state, the evidence behind it, and a record of every command that changed the system are visible
at once. The conflict mode no longer calls itself blocked.

## The Mode Is Not a Failure

`another launcher already represents this application` replaces
`blocked: another launcher already represents this application`. The owner was right: nothing is
wrong when two launchers want one identifier; the tool is simply waiting for a decision. The word
was the tool's, so it was changed in `plan()` where the mode is set, and every consumer followed:
`plan`, `install`, `explain --json`'s `mode`, the activator's `Integrate will` row, and the
Status tab. `tests/90-integration-conflicts.sh` asserts the exact string.

## Three Logs

The panel is a `GtkNotebook` of three pages, each a large read-only monospace text view.

| Tab | Holds | Filled by |
| --- | ----- | --------- |
| **Status** | the current state: the mode in the tool's own words, the launcher, the file, the already-installed notice, and, while a choice is open, the launcher list with the policy wording, and any failure note | `compose_status()`, refreshed whenever the state changes |
| **Discovered** | the evidence: the command that produced it, then detection, size, payload offset and size, compression, version and its source, signature, identifier, embedded entry, desktop id, installed path, icon, exec, window class, this run's mode, the embedded entry text, and every existing launcher with its origin and whether its target exists | `compose_discovered()`, plus the full `explain` report appended by Inspect |
| **Actions** | a timestamped line per command that changes the system, with the command's output underneath: installs, and Run once or Run now | `log_action()` from `run_install` and `run_the_appimage` |

The "This application is already installed." block was removed from above the buttons; it is the
first paragraph of the Status tab, which is the page the window opens on. Inspect and Integrate
still switch the panel to the page that answers them: Discovered for Inspect, Actions for a
command that writes something.

## One Call, Not Two

The Discovered tab needs the payload offset and the signature, which `explain --json` did not
carry, while the human `explain` did. Rather than running both commands, the plan now carries
`payload_offset` and `signature` and `explain --json` emits them, so the window composes the whole
log from the call it was already making.

## Verifying It

A run driven by `--activate` now prints the three logs after performing its actions, because the
logs are the only place the window's knowledge lives and a test cannot read a screen. The test
asserts that Status carries the already-installed notice, that Discovered lists the identifier and
the existing launchers, and that Actions logs the install command including the typed name.

Against the real OrcaSlicer AppImage, whose file the owner moved, the window now says in Status:

```
State:     repair the launcher (the AppImage is not where it was)
Launcher:  com.orcaslicer.OrcaSlicer.desktop
...
  origin:    this tool (this AppImage, and its launcher is broken)
  state:     the same AppImage, not where this launcher expects it
  appimage:  ~/Applications/OrcaSlicer_...AppImage  [MISSING]

Replace existing repairs that launcher: the AppImage is not where the launcher expects it, so
its Exec and TryExec are rewritten.
```

and in Discovered:

```
  Detection         type-2
  Size              131.4 MiB  (137759224 bytes)
  Payload           offset 944632 bytes, size 136814592 bytes
  Compression       zstd
  Version           2.4  (from filename)
  Signature         (absent)
  Identifier        57434599bed20bde
```

which is the deduction the owner asked to see.

## Follow-Up: the Tool's Report Is Status Too

The first version of the tabs left one block above the buttons: the tool's own report, which
`explain --json` carries in its `error` field — "N existing launcher(s) already represent this
application: <paths> choose --replace … or --add …". The owner sent a screenshot of exactly that
block and said it belongs in the Status tab box. It does, so the label was removed from the widget
tree and the text is now part of the Status log, between the already-installed notice and, when
the prompt is open, the detailed launcher list.

Status therefore reads, for the real FreeCAD 1.1.3 AppImage:

```
State:     another launcher already represents this application
Launcher:  org.freecad.FreeCAD.desktop
File:      ~/Downloads/Applications/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage

This application is already installed.
Integrate will show details, then offer to replace or add alongside.

2 existing launcher(s) already represent this application:
  ~/.local/share/applications/org.freecad.FreeCAD-2.desktop  (this tool)
  ~/.local/share/applications/org.freecad.FreeCAD.desktop  (this tool (a different AppImage for the same identifier))
choose --replace to back them up and install this version in their place, or --add to install alongside them
```

`tests/95-activator-ui.sh`'s stub now reports an `error` alongside its conflicts, and the test
asserts that both the report line and the two choices appear in the driven run's Status output.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed. The installed activator was
run against the real AppImage on a virtual display with `--activate integrate` and
`--activate inspect`, and the three logs above are its own output.

## Commits

- `dc331a1` Give the activator Status, Discovered, and Actions tabs
- `5bc7a39` Move the tool's report into the Status tab
