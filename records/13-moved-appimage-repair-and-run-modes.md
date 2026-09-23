# Record: a moved AppImage, the four run modes, and three bugs the question found

## Outcome

The owner removed `$HOME/Applications` on purpose and asked where an integrated AppImage should
live, whether the directory is recreated, whether broken launchers are reported, and whether the
four operations can be told apart. Answering those exposed three real defects:

- a moved AppImage was reported as **a different AppImage** wanting the same identifier, instead
  of the same AppImage that needs its launcher repaired;
- `install --replace` **failed** when the conflicting launcher sat at the target identifier;
- the install plan was printed **two or three times**, because forked cache-refresh children
  flushed the inherited copy of the process's stdout buffer.

All three are fixed, every run now names its operation, and `list` marks records whose AppImage
is gone.

## Answers, as the Code Now Stands

**Where should an AppImage live?** There is no XDG directory for AppImages. The XDG Base
Directory Specification defines data, config, cache, state, and runtime directories; it defines
none for executables, and an AppImage is an executable. The default here is `$HOME/Applications`,
the de-facto convention from AppImageLauncher, and `--install-dir DIR` replaces it — including
with `"$XDG_DATA_HOME/AppImages"`, which is the defensible XDG-shaped choice because an AppImage
is user data. The launcher and the icons do go to `$XDG_DATA_HOME`, where the specification puts
them.

**Is the directory created when missing?** Yes. The step that places the AppImage calls
`create_directories` on the target's parent, so removing the whole directory and integrating
again recreates it. `tests/90-integration-conflicts.sh` removes the directory with `rm -rf` and
asserts that the repair puts both the directory and the AppImage back.

**Are broken launchers reported as such?** Now in two places, where before only one:

- `audit` reported them already: `Exec target is missing` and `TryExec target is missing`, as
  errors, per launcher.
- `list` marks the record `[MISSING: the launcher points at a file that is not there]`, and
  `explain --json` reports the conflict's `exec_exists` as false, which the activator renders as
  `[MISSING]`.

A record whose `identifier` is empty is skipped by `list`: the handler keeps its own record in
the same directory and was being listed as an empty installation.

**Can the four operations be told apart?** Every plan now carries a `mode`, printed as
`this-run:` by `plan` and `install`, and as `"mode"` by `explain --json`; the activator shows it
as the `Integrate will` row and describes each existing launcher's state:

| Operation | `this-run:` |
| --------- | ----------- |
| registering a new AppImage | `new integration` |
| updating an already integrated AppImage in place | `update the launcher in place` |
| repairing a moved AppImage | `repair the launcher (the AppImage is not where it was)` |
| replacing an older AppImage with a newer one | `blocked: …` with no policy, `replace an existing launcher` with `--replace`, `add alongside as <id>` with `--add` |

## Repair, Not a Takeover

`detect_application_conflicts` treated a launcher as ours only when its `Exec` named the very
file being installed. Anything else at the same identifier became
`this tool (a different AppImage for the same identifier)`, which is wrong for the same AppImage
at a new path.

The identifier is a hash of the embedded entry, the file size, and the payload offset, so it
survives a move. The detection now also accepts a launcher whose owning record carries the new
identifier, and marks that conflict `repair`. Its origin says what is actually true —
`this tool (this AppImage, and its launcher is broken)` when the recorded path is gone, or
`this tool (this AppImage, now at a different path)` when it is not — and installing it
recreates the managed directory, returns the AppImage to it, and rewrites `Exec` and `TryExec`.

A different build has a different identifier, so it is still a conflict that needs an explicit
`--replace` or `--add`.

## Two Bugs in the Path of That Question

**`--replace` refused to replace.** With the conflicting launcher at the target identifier and
not an upgrade, the write guard in `install()` saw an existing file that the plan was not
"upgrading" and refused with "a desktop entry already exists … choose --replace", even though
`--replace` was exactly what had been given. The guard now also accepts a target the plan is
taking over through `replace_conflicts`. `tests/90` covers the whole replace-and-restore cycle
for two builds of one application.

**The plan printed two or three times.** `run_command` forks, then `freopen`s stdout to
`/dev/null` before `execvp`. The `freopen` flushes the stream, and the child had inherited the
parent's unflushed `std::cout` buffer, so the plan text was written to the real stdout again by
each child. `run_command` now flushes `std::cout` and `std::cerr` before forking, and the test
asserts that a successful install prints `this-run:` exactly once.

## Tests

`tests/90-integration-conflicts.sh` gained a five-step sequence on its own second application:
a new integration, an update in place that must print one plan, a removed managed directory
whose record is marked `[MISSING]` and reported by `audit`, a repair from the new path that
recreates the directory and rewrites the launcher, and a newer build that is blocked, replaced
with a backup, and restored by `uninstall`. `tests/70-integration-sandbox.sh` asserts that the
handler's record is not listed as an AppImage.

## State on This Host

`~/Applications` is gone by the owner's design and the AppImages are at
`~/Downloads/Applications`, so all seven records are marked `[MISSING]` and `audit` reports the
missing `Exec` and `TryExec` targets for each launcher. Nothing was changed on the owner's disk.
`appimage-integrate plan --install-dir ~/Downloads/Applications <AppImage>` shows the repair that
would adopt the new location without moving anything:

```
this-run: repair the launcher (the AppImage is not where it was)
```

## Verification

Verified 2026-09-23 with `make test`; all fifteen test scripts passed. The real state was read
with the installed `appimage-integrate list` and `audit`, and the repair plan was previewed with
`plan` alone.

## Commits

- `50473c2` Repair a moved AppImage and say which operation a run performs
- `e837fb6` Do not list the handler's own record as an installed AppImage
