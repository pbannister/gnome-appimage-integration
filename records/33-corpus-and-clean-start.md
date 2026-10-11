# Record: the corpus, and what a clean start found

## Outcome

The tools were run against 29 popular AppImages, each from its own throwaway desktop, and the
desktop they ran on was reset to a clean start first and put back afterwards.  All 29 integrated.
The corpus confirmed the symlinked root desktop entry of record 32 against five real applications,
and found four things worth fixing -- one of which had nothing to do with AppImages at all:

| Finding | What was done |
| ------- | ------------- |
| An aarch64 and a 32-bit AppImage integrated silently on an x86-64 machine | `plan` and `install` now warn; a 32-bit image on a 64-bit host is a note, because that one does run where the kernel supports it |
| The project no longer compiled once GTK 4.18 or newer development files were present | `gdk_x11_surface_get_xid` is deprecated in 4.18 with no replacement, and `-Werror` made it fatal |
| `tests/94-window-class.sh` copied the host's `sleep` as a stand-in program | the probe is compiled instead, because coreutils is now one binary that dispatches on the name it was invoked as |
| The new reset and restore scripts damaged the handler and mis-parsed a record with an empty class | both fixed, each with the test that now holds it |

## The Apparatus

| File | Role |
| ---- | ---- |
| `scripts/corpus-list.tsv` | 29 applications, with a current download URL and size, or a `local:` source naming a file already on the machine |
| `scripts/corpus-fetch.sh` | fetches the corpus, skipping what is cached and copying what is local |
| `scripts/lib/corpus-paths.sh` | decides where the samples, reports, and per-application sandboxes go |
| `scripts/corpus-run.sh` | runs the tools over the corpus and writes the catalogue |
| `scripts/live-state-snapshot.sh` | writes a restore point: what `install` needs to reproduce each integration |
| `scripts/live-state-reset.sh` | uninstalls everything recorded and sweeps what no record names, after writing a restore point |
| `scripts/live-state-restore.sh` | puts the integrations back from a restore point and checks each launcher |
| `tests/06-live-state.sh`, `tests/07-corpus-run.sh` | hold the three state scripts and the runner |

Thirteen of the 29 were already on this machine, so the list names them with a `local:` source and
the fetcher copies them: the corpus costs 2.55 GB to download rather than 5.58 GB, and no absolute
home path reaches the repository.  The remote half is resolved from each project's own release
rather than trusted to the AppImage catalog, whose entries are often years old -- OpenSCAD's catalog
entry, for instance, still points at a 2019 build.

The corpus is not part of `make test` and should not be: it is gigabytes, it needs the network, and
it takes minutes.  `tests/07-corpus-run.sh` holds the runner hermetically with synthetic AppImages
instead, including the property that matters most here -- a second run over the same corpus produces
a byte-identical catalogue, so nothing is inherited between applications or between runs.

## What the Corpus Found

| Shape | Count |
| ----- | ----- |
| Integrated (`ok`) | 28 |
| Refused correctly, a system launcher already represents the application | 1 (OpenSCAD) |
| Installed | 29 of 29 |
| Compression: gzip / zstd / xz | 15 / 12 / 2 |
| Root desktop entry: real file / symlink / absent | 24 / 5 / 0 |
| `usr` is a symlink to `.` | 2 (Audacity, MuseScore) |
| `Terminal=true` | 2 (Neovim, appimagetool) |
| No `StartupWMClass` in the embedded entry | 11 |
| A class in the embedded entry that nothing can check | 18 |
| A real signature (not empty padding) | 2 |
| Update information present | 7 |
| Built for another machine | 2 (aarch64 FreeCAD, 32-bit balenaEtcher) |

The five symlinked root entries -- Audacity, Cryptomator, MuseScore, Tiled, appimagetool -- are the
fix from record 32 holding up against real applications rather than a fixture.  The two `usr -> .`
payloads exercise the directory-link half of that fix.

**The finding that is not fixed, because it cannot be at install time.**  Eleven of the 29 name no
window class, and eighteen more name one that nothing local can check; the dock matches a launcher
to a window by that class, so for roughly a third of popular AppImages the launcher cannot be right
until someone reads the class from the running window.  The tools warn, in both cases, and name
`appimage-integrate windows` and `--wm-class`/`--wm-class-from-window` as the way to find it.  The
corpus also makes the size of that gap concrete for the first time: it is not an OrcaSlicer
peculiarity, it is the common case.  Guessing a class from the desktop id or icon name would be
worse than saying nothing, because a wrong class is as useless as none and harder to notice.

## Decisions

- **A reset writes a restore point first and refuses to run without one.**  A reset that cannot be
  reversed is not a test aid, it is an accident.
- **A reset leaves the `*.AppImage` handler registered, and its record in place.**  The handler is a
  desktop registration the owner chose, not per-run state; removing the entry while deleting the
  record was the first version's mistake, and it made `handler uninstall` fail.
- **A reset is narrow.**  Another tool's `appimagekit_*` definitions, the shared `appimage.xml`, and
  unrelated launchers are left alone.
- **An architecture mismatch is a warning, not a refusal.**  Integrating a file for another machine
  is sometimes deliberate -- preparing for another device -- so the tool says what it knows and
  proceeds.
- **A system launcher is a conflict, not an error.**  The tool already refuses to shadow a launcher a
  package wrote; the corpus records that as a finding and takes the `--add` path.
- **The corpus list carries no absolute home paths.**  A local source is written relative to `$HOME`.

## Verification

verified 2026-10-10:

- `make test` passes every script, including `tests/06-live-state.sh` and `tests/07-corpus-run.sh`.
- The corpus run: 29 applications, 29 installed, one of them through `--add`, and the architecture
  warning fires for the aarch64 build.
- The live reset: 11 integrations uninstalled and 261 leftover files swept; verified afterwards that
  `list` was empty, no integration icon or MIME definition remained, the handler still answered, and
  `appimagekit_*`, `appimage.xml`, and all 16 AppImages in `~/Applications` were untouched.
- The live restore: 11 of 11, each checked against the record; the chosen classes came back
  (`orca-slicer`, `UltiMaker-Cura`, `openshot`, `FreeCAD`, `NETGEAR Discovery Tool`), and Subsurface
  has no class, as it had none before.
- `tests/06-live-state.sh` was verified both ways for the tab-collapsing parse: it passes with the
  fix and fails against the reverted script, as does `tests/05-release-publish.sh` for the push
  check.
- `tests/70-integration-sandbox.sh` patches the ELF machine field of a synthetic AppImage and
  asserts the warning, so the fixture needs no foreign binary.

## Commits

- `7dce1bf` Build against GTK 4.18 and later
- `aacd506` Snapshot, reset, and restore the desktop's integration state
- `4fd5133` Leave the handler entry registered when the state is reset
- `df3cd19` Keep the handler's own record when the state is reset

- `b2e9951` Add the corpus fetcher and runner, and test the runner
- `149c7e7` Add the corpus list, and let it name a file already on this machine
- `e7ae278` Compile the window-class probe instead of copying the host's sleep
- `96d058e` Warn when an AppImage is built for another machine
- `5f8fbee` Restore a record whose window class is empty

## If It Happens Again

To run the corpus again, from a clean start and back again:

```
make build
sh scripts/live-state-reset.sh          # writes a restore point, then resets
sh scripts/corpus-run.sh                # catalogue under the corpus directory's reports/
sh scripts/live-state-restore.sh        # puts the integrations back
```

`reports/edge-cases.txt` is the summary to read first: outcomes, warnings by text, and the shapes.
The per-application raw output is beside it, in `<app>/inspect.txt`, `plan.txt`, and `install.txt`.
`scripts/lib/corpus-paths.sh` prints both directories, or `APPIMAGE_CORPUS_DIR` and
`APPIMAGE_SANDBOX_DIR` set them.

Two environment changes cost time in this episode and are worth recognising: a newer GTK turns a
deprecation into a build failure because the project builds with `-Werror`, and a swapped-out
coreutils turns a copied binary into a program that refuses to run.  Neither is a fault in this
project, and both look like faults in it until the environment is checked.

## Amended 2026-10-10

The collected samples now live on this machine's bulk storage, `/datasets/appimage-corpus`, rather
than in the home directory: 5.2 GB of AppImages belongs on the 5 TB volume, not beside the operating
system.  `scripts/lib/corpus-paths.sh` makes that decision in one place -- `/datasets` when it
exists, `$HOME/appimage-corpus` otherwise -- and the per-application sandbox moved the other way, to
`${TMPDIR:-/tmp}`, because a spinning disk should not be seeking through a throwaway desktop for
every application while the run reads the samples.  The corpus was moved and checked path by path
and size by size before the home copy was removed, and `/datasets/manifest.yml` records it beside
the other datasets.  The paths in this record are the ones to use; where it says
`~/appimage-corpus`, read the corpus directory that the scripts choose.
