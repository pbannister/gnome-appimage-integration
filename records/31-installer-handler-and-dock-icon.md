# Record: the handler the installer did not install, and the OrcaSlicer icon

## Outcome

A fresh machine now comes out of the one-command install with a working `*.AppImage` handler:
the installer runs `handler install`, and the handler step survives a machine that has never had
a user launcher or an AppImage MIME definition.  `plan` and `install` warn when the window class
they would write is only the AppImage's own guess, and the README says how to read the real one.
Release **v2026.10.02** is published from `af16924`, and `release-publish.sh` no longer skips
pushing the commit it is releasing.

## What Happened

Two reports from a laptop that had run the published installer: the `*.AppImage` MIME handler was
not installed, and integrating the OrcaSlicer AppImage left no icon in the dock while it ran.

There were three separate causes, and only the first is about the release being old.

**1. The published release was 14 commits behind `master`.**  The only release was `v2026.09.23`,
tagged at `6171dd2` (2026-09-23 17:39); `master` was at `15af8b1`.  Commit `3baf79c` — *Keep a
window class this tool installed, and repair two launchers*, written up in
`records/28-window-class-lost-and-kept.md` — landed after the tag.  Downloading the released asset
confirms what the laptop ran: it reports `2026-09-23-master-v2026.09.23-6171dd2`.  That release
writes the embedded `StartupWMClass=OrcaSlicer`, whose window reports `orca-slicer`, so the dock
has nothing to match and shows no icon.

That fix alone would **not** have repaired the laptop, and this is the part worth remembering: step
4 of the class order keeps a class *already on disk*, so it does nothing on a machine that has never
integrated the file.  A first integration still writes the embedded guess, because the class a window
really reports is only knowable while the application runs.  The recovery is `appimage-integrate
windows` while it runs, then `install --wm-class-from-window`.

**2. The installer never registered the handler.**  That was deliberate — `install.sh` printed the
opt-in step and record 26 recorded the decision — but a user who installs the tool has asked for the
tool, and a machine that had never run the second command had no handler at all.

**3. Three defects broke the handler step on a machine that had never had one.**  They were invisible
on the owner's desktop, where earlier work had already left the directories and the definition in
place:

- `$XDG_DATA_HOME/applications` was never created.  On a machine without it the entry write failed,
  `write_text_file`'s result was discarded, and `handler install` went on to record a MIME default
  for an entry that was never written.
- The AppImage MIME definition was only ever *rewritten* when it already existed.  A machine with none
  kept the system's generic icon for AppImage files, and a system whose `shared-mime-info` predates
  the AppImage types had no file type at all.  The owner's desktop had a definition left by earlier
  work, which is exactly why the two machines looked so different.
- The release tarball shipped the activator icon only under `share/icons/`, while
  `handler_icon_source` looked beside the tool (`bin/icons/`, then `bin/`).  `make install` copies the
  icon to both places, so the source layout worked and no release install could install the handler's
  icon.  `tests/03-release-install.sh` unpacked the tarball but never ran `handler install` from it.

## Decisions

- **The installer registers the handler.**  This reverses record 26's opt-in decision, which is why
  that record now carries a correction.  `APPIMAGE_INTEGRATION_NO_HANDLER=1` installs the tools and
  registers nothing, and the report prints how to reverse the registration.
- **The MIME definition this tool writes is the tool's to remove.**  `handler install` writes it only
  when none exists and records `mime_created=1`; `handler uninstall` deletes it.  A definition that was
  already there is backed up and restored instead, never deleted — and a second `install` carries the
  `mime_created` fact forward from the record, because the second pass finds the definition already
  correct and has nothing to rewrite.
- **The warning is for the guess only.**  A class chosen with `--wm-class`, read from the running
  window, or adopted from another launcher this tool wrote does not warn; only the embedded entry's
  own value does, because nothing can check it until the window exists.
- **A failed handler step does not fail the install.**  The tools are installed by then; the installer
  reports what the tool said and prints the command to finish the job.

## Verification

verified 2026-10-02:

- `make test` passes every script, on the committed tree (`af16924`), with no failures.
- `tests/91-handler-mime-definition.sh` is new and is the regression test for the MIME definition and
  the applications directory.  It failed against the code before the directory fix — which is how that
  defect was found rather than reasoned about — and now covers: the definition written when absent, a
  second install keeping it recorded as this tool's, `handler uninstall` removing it, and a definition
  that was already there restored byte for byte.
- `tests/03-release-install.sh` asserts `./bin/icons/appimage-activator.svg` is in the tarball, that the
  installer registers the handler and installs its icon into a sandboxed `HOME`, that it writes the
  definition on a machine with none, and that `APPIMAGE_INTEGRATION_NO_HANDLER=1` leaves the handler
  unregistered.  Every installer run in that test is sandboxed, so the real desktop is untouched.
- `tests/94-window-class.sh` asserts the embedded class warns and names `--wm-class-from-window`, and
  that an explicit class does not warn.
- `tests/05-release-publish.sh` is new and is the regression test for the push check: a clean clone, a
  local bare repository holding the branch one commit behind, and a stubbed `gh`.  It was verified both
  ways — PASS with the fix, FAIL against `release-publish.sh` reverted to the old check.
- Live: `v2026.10.02` is published at
  <https://github.com/pbannister/gnome-appimage-integration/releases/tag/v2026.10.02>, and `master`
  carries `af16924` and `670864e`.

One observation, so a later session is not puzzled by it: a rebuild from `af16924` *after* the tag was
created reports `2026-10-02-master-v2026.10.02-af16924`, because the version carries the tag when the
commit has one.  The published tarball was packaged before the tag existed and reports
`2026-10-02-master-af16924`.  Both name the commit.

## Commits

- `af16924` Register the handler from the installer, and define the AppImage types
- `670864e` Push the commit being released, not just the tag

## If It Happens Again

A dock icon that is missing or wrong is a mismatch between the launcher's `StartupWMClass` and the
class the window reports, and only the running application knows the second one:

```
appimage-integrate windows                       # the class each running AppImage reports
appimage-integrate install --wm-class-from-window <AppImage>
```

`audit` reports launchers with no class at all, and `plan` and `install` now warn when the class they
would write is the embedded entry's.  Check `handler status` before believing the handler was never
installed: the entry, its icon, and the MIME definition are three separate things, and `handler
install` now fails loudly rather than silently when it cannot write the entry.

## Correction (2026-10-02)

The OrcaSlicer attribution above was checked against the AppImage actually installed on this machine
after the record was written, and it is wrong for that file.  Its embedded desktop entry names **no**
`StartupWMClass` at all:

```
[Desktop Entry]
Name=OrcaSlicer
Exec=AppRun %F
Icon=OrcaSlicer
Type=Application
PrefersNonDefaultGPU=true
X-KDE-RunOnDiscreteGpu=true
Categories=Utility;
MimeType=model/stl;application/vnd.ms-3mfdocument;application/prs.wavefront-obj;application/x-amf;
```

So for that file the dock-icon failure is the *absent* class, not a wrong one, and the stale release
is not what caused it: the published release and `master` behave identically there, and the older
warning — "the embedded entry has no StartupWMClass and no earlier launcher supplied one" — already
said so.  The `StartupWMClass=OrcaSlicer` value repeated above from record 28 came from a build whose
entry named it, or from the launcher as it stood then; the V2.4.2 file does not name one.  What the
new warning adds is the other case, an embedded class that does not match; both warn, and both point
at the same recovery.

Verified 2026-10-02 against the real file with `plan`, which writes nothing:

```
$ appimage-integrate plan ~/Applications/OrcaSlicer_..._V2.4.2_....AppImage
startup-wm-class: (none)
warning: the embedded entry has no StartupWMClass and no earlier launcher supplied one; ...

$ appimage-integrate plan --wm-class orca-slicer ~/Applications/OrcaSlicer_..._V2.4.2_....AppImage
startup-wm-class: orca-slicer  (set explicitly, kept over the embedded entry)
```

The published `v2026.10.02` tarball carries the README as it stood at `af16924`, whose OrcaSlicer
paragraph says the entry names `OrcaSlicer`.  The wording on `master` is corrected; the artifact is
not repackaged, because a rebuild from `af16924` now reports the tag inside its version and would no
longer match the tag it is published under.

