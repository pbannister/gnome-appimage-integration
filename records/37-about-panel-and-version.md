# Record: an About panel, and the version it exposed

## Outcome

The window has a fourth panel.  **About** carries the program's name, the project it belongs to,
the build-time version, and a clickable link to the repository:

```
=== tabs: Status Discovered Actions About ===
=== About ===
AppImage Activator
gnome-appimage-integration
2026-10-21-master-1211e2d (build 656)
https://github.com/pbannister/gnome-appimage-integration
```

The version is whatever the shared `version` module reports, so the panel and the command-line
tools cannot disagree.  The page is built from widgets rather than a text view — a heading, a
dimmed project name, the version, and a `GtkLinkButton` — because a link a person has to copy out
of a text area is not a link.

## What the panel exposed

The version read:

```
2026-10-21-master-v2026-10-15-master-1211e2d-1211e2d-changes
```

which is not a version.  It is a compounding loop:

- `release-publish.sh` names a release `v$VERSION` when the commit carries no tag, and `VERSION`
  is the build-time version, `date-branch-hash`.
- `version-generate.sh` appends any tag that is on the current commit.
- So each published release embeds the previous release's version inside the next one, and the
  one after that embeds that.  The three tags on this repository's recent commits —
  `v2026-10-15-master-6bde637`, `...-1a761f0`, `...-1211e2d` — are the loop mid-flight.

The two scripts were each doing what they say; the fault is in the interaction.  The minimal
repair is at the reading end: a tag that is itself shaped like a generated version is not
appended.  A human release tag, `v2026.10.03`, is not shaped like one and is still appended, which
is the case the `date-branch-tag-hash` form was written for.

`release-publish.sh` still invents `v<version>` tags, so the repository accumulates tags named
after versions.  That is now harmless to the version string, but it is worth a decision of its
own — inventing a tag named after a build-time fact makes the tag a copy of something derived
from git state rather than a label a person chose.

## Verification

- The work and its tests are committed as `dcf95c6`, and everything below was run on that tree.
- `tests/16-version-generate.sh` runs the real generator in a throwaway git repository: no tag,
  a human release tag appended, a generated tag not appended, the next release not compounding,
  and `-changes` for a dirty tree.  Written against the old behaviour it fails with exactly the
  reported symptom (`...main-ee744cd` becoming `...main-v2026-10-21-main-ee744cd-ee744cd`), which
  is how the guard was checked.
- The fixture's own traps were found the same way: the copied script was untracked, so every
  version carried `-changes`; and the test's second release case tagged with the *previous*
  commit's version, which is not the order the release tool uses.
- `tests/95` asserts the tab list and the panel's four lines, so a panel that exists nowhere
  fails the test rather than being noticed by a person.
- `make test` passes.

## Amended: the version is asked for, not compiled in

The panel first showed the build-time version this window was compiled with.  It now shows the
output of `appimage-inspect --version`, run when the panel is built:

```
appimage-inspect 2026-10-21-master-73a25cb (build 691)
```

The reason is the one that made this record worth writing.  The window, the tool it drives and
the inspector are separate binaries that can be separate builds -- the installed pair and the
build tree disagreed about the version more than once while this was written -- and a string
compiled into the window says what the window was built from, not what is installed.  Asking the
inspector answers the question a person is actually asking, and the answer is checkable: it is
the same line the command prints in a terminal.

The inspector is looked for beside the tool the window was told to drive, which is where both are
installed: `bin/` in a release, `dataflow.out/build` in a build tree.  When there is none, the
panel says so rather than falling back to a compiled-in number that might be a different build:

```
appimage-inspect --version: not found beside /tmp/.../alone/appimage-integrate
```

`tests/95` gives the stub tool a sibling inspector printing a version no build of this project
could have, and asserts the panel shows exactly that; a second run with the tool alone asserts the
found-nothing message.
