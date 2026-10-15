# Record: the specification's opt-out, and AppStream metadata

## Outcome

Two things the AppImage specification describes were read again and acted on.

The specification lists five conditions under which desktop integration **SHOULD NOT** be
attempted.  They were already documented in `documents/07` and `documents/10` and implemented
nowhere.  They are now **detected and reported as warnings, and nothing is refused**: the
conditions exist to stop unsolicited integration, and a person who clicks Integrate has given the
permission the same section of the specification asks for.  This follows the project's principle
that a warning is not a gate.

The specification also ships AppStream metadata so an application is discoverable in an
application centre.  The project read it only to scrape a version out of it.  It is now installed
for the user, recorded, and removed on uninstall -- including the case the corpus turns up, where
two integrations of one application share one file because AppStream requires the file to be
named after the component rather than after anything this tool chooses.

`firejail` was considered and deferred, and the container-shaped alternatives were assessed in
`documents/13-sandboxing-appimages.md` rather than implemented.

## The opt-out

| Condition | Check |
| --------- | ----- |
| `$XDG_DATA_HOME/appimagekit/no_desktopintegration` | path from the environment, `$HOME/.local/share` fallback |
| `/usr/share/appimagekit/no_desktopintegration` | constant |
| `/etc/appimagekit/no_desktopintegration` | constant |
| a process named `appimaged` is running | `/proc`, reusing the scan the `windows` command already had |
| `$DESKTOPINTEGRATION` is non-empty | environment |

The detection is a pure function, `desktop_integration_optouts()`, so that every condition
including the two paths no test can create under `/usr` or `/etc` is exercised by
`tests/desktop_integration_optout_test.cpp`.  The process table is the command-line tool's
business rather than the library's, so its answer arrives through `integration_options_o`.

Every condition that holds becomes a `warning:` line in `plan`, `explain` and `install`, an entry
in the JSON report, and a warning block at the top of the window styled with the `warning` class
and a `dialog-warning-symbolic` icon -- an icon and words, not colour alone.  Integrate is not
disabled, and `tests/95` holds that: with a condition reported, the block is up and Integrate is
still the suggested action.

The integrator captured only `HOME` and `XDG_DATA_HOME` from the environment, so
`$DESKTOPINTEGRATION` had to be added to what it carries.  `tests/71` found that: the test asserted
the warning and got none.

## AppStream metadata

Fourteen of the corpus's 29 images carry metadata, and the ways they carry it set the rules:

| What the corpus does | The rule it produced |
| -------------------- | -------------------- |
| Subsurface ships `subsurface.metainfo.xml` and declares `org.subsurface_divelog.Subsurface` | the identity is the `<id>` element, never the file name |
| Cryptomator ships both `.appdata.xml` and `.metainfo.xml` for one component | the modern suffix wins; one file per component is installed |
| Cryptomator also ships a `.gitkeep` in the directory | anything that is not metadata is left where it is |
| Tiled declares an id ending in `.desktop` | the id is sanitised into a file name rather than trusted as one |
| FreeCAD ships one component id in both its x86-64 and its aarch64 images | the file is shared, and uninstall removes it only when no other record still claims it |

The file goes to `$XDG_DATA_HOME/metainfo/<id>.xml`, which is where AppStream keeps
per-application metadata for a user.  Three ownership rules make it reversible:

- A file another record already names belongs to that integration, so it is overwritten without
  keeping a copy: that record puts its own file back when it goes.
- A file no record names belongs to somebody else -- the distribution's, say -- so it is copied
  aside at install and restored at uninstall.  The first version of this kept a copy in both
  cases, and `tests/72` caught the leak: the last uninstall restored a file whose owner had
  already been removed, leaving an orphan behind.
- Uninstall leaves the file alone when another record still names it.

When the metadata's `<launchable type="desktop-id">` names a launcher this tool did not write, the
plan warns.  The corpus supplies a real instance: OpenSCAD's metadata launches `openscad.desktop`,
and integrating alongside the distribution's existing launcher writes `openscad-2.desktop`, so the
component and the launcher are not connected.  The metadata is not rewritten to hide it -- the
mismatch is a property of installing alongside, and saying so is more useful than a launcher that
points at an identity a later distribution update may take away.

The catalogue cache is refreshed with `appstreamcli`, which renamed the command: AppStream 1.0
spells it `refresh --cache` and earlier releases `refresh-cache`.  Both are tried, whichever fails
quietly, and a failure is not reported -- the next refresh picks the file up.

## The container question

`firejail` is not installed here and neither, in this environment, is a user systemd bus, so
`systemd-run --user` could not be verified either; `bwrap` is the only candidate that runs on this
machine today, and it ships no AppImage profile.  `systemd-nspawn` needs an operating-system tree
and root for one GUI application, which is the wrong shape for a launcher, and the container
runtimes are heavier still.  The assessment, and the `Exec` decision that has to be settled before
any of it is written, are in `documents/13-sandboxing-appimages.md`.

## The Apparatus

| File | Role |
| ---- | ---- |
| `sources/integration/appimage_integrator.{h,cpp}` | `desktop_integration_optouts()`, the AppStream collection, install, ownership, and uninstall |
| `sources/tools/appimage_integrate.cpp` | `process_named_is_running()`, the opt-out and AppStream fields in the JSON, the warning in the install's output |
| `sources/tools/appimage_activator_ui.cpp` | the warning block, and the report line a test reads it by |
| `tests/desktop_integration_optout_test.cpp` | every condition, including the two system paths |
| `tests/71-desktop-integration-optout.sh` | each condition end to end, warned about and never obeyed |
| `tests/72-appstream-metadata.sh` | install, record, share, restore, and uninstall |
| `scripts/corpus-run.sh` | two new catalogue columns: the components a payload carries, and the conditions reported |

## Verification

- The work and its tests are committed as `4fff2a7`, and everything below was run on that tree.
- `make test` passes, including the two new tests.
- Each new test was written to fail against the code before the fix: the opt-out test found the
  uncaptured `$DESKTOPINTEGRATION`, and the AppStream test found the orphaned backup.
- Every integration test now unsets `$DESKTOPINTEGRATION`, so an ambient value cannot add a warning
  to a test that is asserting on something else -- the same class of environment trap that cost
  time in records 31 and 33.
- The corpus was re-run: 29 applications, 28 `ok` and 1 installed alongside an existing launcher,
  29 installs verified, 14 of them carrying AppStream metadata, and no application reporting an
  opt-out condition.
