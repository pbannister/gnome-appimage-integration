# TODO

## Pending

* [ ] use a zsync client for updates when one is installed, so an update transfers a delta instead of the whole file. `zsync`, `zsync2` and `appimageupdatetool` are all absent here, so `update` downloads in full today; the check already prints the size.
* [ ] decide what to do about the version a file name states: `version_of_appimage` reads OrcaSlicer's `…V2.4.2_62a8fff…` as `2.4`, which is what an update check would compare against a release tag.

## Open Questions

* [ ] improve the human-oriented documents in `documents/`.
* [ ] read the tool-universe sources in `documents/02-tool-universe.md`.

## Recently Completed

* [x] actually update an AppImage: `update` downloads to `<name>.part`, verifies the download against the release's published `-SHA256.txt` and the file's own `.sha256_sig`, refuses a mismatch or an unreadable file and leaves the working one alone, then replaces the file, makes it executable and re-renders that one launcher (record, icon, name and class kept). `--force` takes the offered file when the transport names no version, `--backup` keeps the old file as `<name>.previous`, and `--dry-run` prints the URL and size without fetching.
* [x] `migrate` adopts a launcher another tool wrote: it takes the AppImage from the launcher's `Exec`, installs this tool's launcher for it, backs up the displaced one, and steals its window class. A dry run, an ID or a path, JSON, and a clear refusal when the AppImage is gone are all covered.
* [x] fix the dead recovery path for `StartupWMClass`: the class is now borrowed from a conflicting launcher after those conflicts are detected, and the detection is repeated with the adopted class, so "adopted StartupWMClass=…" fires. `desktop_exec_program` also moved into the desktop reader, where the Exec parsing belongs.
* [x] deal with the broken `balena-etcher-electron.desktop`: the AppImage was in `~/Downloads/Installers`, so it was re-integrated from there; the tool's repair path moved it back to `~/Applications`, re-rendered the launcher with the full action set, and the audit finding cleared.
* [x] put the update information to work: `appimage-inspect --update-url` resolves the field offline into the URL and asset pattern it names; `update --check [--all]` asks the GitHub API (release, prerelease, or a specific tag) or reads a zsync file's `Filename:` header, and compares the answer with the installed version without downloading anything; `audit` reports a recorded AppImage whose update information is missing or unusable, and `audit --check` reports which have an update waiting; every launcher now carries a **Check for updates** context action. Fixing this also exposed an inverted sign in the version comparison, where the shorter of two otherwise equal versions sorted newer (1.0 > 1.0.1).
* [x] add a `refresh` command that re-writes every recorded launcher from the embedded entry. It preserves what the embedded entry does not carry — the `Name=` a user chose, the desktop id, the icon name, and the window class — keeps each recorded launcher's own identifier so a second launcher for one AppImage keeps its own record, and names a launcher whose AppImage is gone instead of failing silently. All eight launchers here now carry the `AppImage Activator` context action.
* [x] re-integrate the launchers that had no class. Cura was read from its running window with `install --wm-class-from-window` (`UltiMaker-Cura`); OpenShot and OpenShot-2 were read the same way with `refresh --wm-class-from-window` (`openshot`, the class half of the window's `openshot-qt`/`openshot` pair), after OpenShot turned out to crash on this session and had to be run under software rendering to be read at all. `windows` now reads the window tree as well, so a window no window manager has marked is still found.

* [x] read `StartupWMClass` from the running application: `appimage-integrate windows` lists each running AppImage with the class it reports (the program inside the mount for any AppImage, an X11 client's `WM_CLASS` when there is a window), `--wm-class-from-window` applies one, a class set with `--wm-class` is recorded as an explicit override that outranks the embedded entry on later installs, and OrcaSlicer was re-integrated with `orca-slicer`.

* [x] verify the `.sha256_sig` section: a hex digest against the SHA-256 of the file with the section zeroed, a PGP signature with `gpg --verify`, a mismatch refused by `install` unless `--ignore-signature` is given, and the result reported by `explain`, `plan`, `appimage-inspect`, `install`, and the activator's Status log.
* [x] remove the last AppImageLauncher artefact, the unreferenced icon in `~/.local/share/icons/hicolor/0x0`, which also cleared the `audit` warning.
* [x] fix the 64-bit ELF section header read, which used the 32-bit field offsets and so reported every section's offset and size as zero.
* [x] open the activator from the launcher's context menu, and establish that the shell's `App Details` item cannot be suppressed from a desktop entry.
* [x] decide the likely button from the version relation and the launcher count: Close for a complete or older file, Integrate otherwise, and Add alongside (with the version in the name) when an older file or several launchers must coexist.
* [x] emphasise the `This run` and `Error` fields in the Discovered log, and order the conflict choice Replace existing, Add alongside, Back.
* [x] order the first view's buttons by likely use (Integrate first) and mark the likely action with the HIG suggested-action style.
* [x] report a complete integration as `properly integrated` in the details row and the Status tab, instead of implying work remains.
* [x] split the activator's text area into Status, Discovered, and Actions tabs, and drop "blocked:" from the conflict mode.
* [x] convert the graphical activator from Python/PyGObject to C++ with GTK4, with a minimal JSON reader and no Python at run time.
* [x] treat a moved AppImage as a repair rather than a different AppImage, repair it in place, and label every run as new, update, repair, replace, or add alongside.
* [x] mark missing AppImages in `list`, and fix `--replace` when the conflicting launcher sits at the target identifier.
* [x] stop forked cache-refresh children from printing the plan again from their inherited stdout buffer.
* [x] make the window's application id match `appimage-activator.desktop`, so the dock shows the activator icon instead of a generic one.
* [x] add a prefilled `Name:` field to the conflict prompt, so `--add` and `--replace` can name the launcher after a version.
* [x] rename the right-click "Open With" item to AppImage Activator, because AppImage Handler is another project's name, and migrate the pre-rename entry, icon, and record.
* [x] make the handler multi-instance, so opening a second AppImage opens a window for that file.
* [x] show every existing launcher, upgrades included, instead of filtering them out.
* [x] treat a different AppImage claiming an owned identifier as a conflict, never a silent takeover.
* [x] keep the existing launcher, its record, and its icon when `--add` installs alongside.
* [x] report in `audit` when a record disagrees with the AppImage its launcher actually runs.
* [x] add a GTK4 activator dialog that remembers its window size between runs (`sources/tools/appimage_activator_ui.cpp` since the port to C++).
* [x] show Name, Comment, GenericName, and the detected version in the handler's first window.
* [x] make Inspect open a separate window with only a Close button, returning to the AppImage window.
* [x] make the Integrate dialog list each existing launcher's id, name, origin, version, target, icon, window class, and whether the target exists.
* [x] extract the application version from `X-AppImage-Version`, then AppStream, then the file name, and report the source.
* [x] add `explain --json` for the graphical handler, and `tests/95-activator-ui.sh` for the script and its dispatch.
* [x] give Run once a persistent notice naming the application and the child process id.
* [x] make Inspect show a non-empty report: container facts, payload root, embedded entry, and the install preview.
* [x] make Integrate show the list of files written.
* [x] detect existing launchers for the same application and offer Replace existing, Add alongside, or Cancel.
* [x] add `--replace` and `--add`, with a manifest record so a replaced launcher is restored on uninstall.
* [x] retire the manifest of a launcher upgraded in place, so `list` stays accurate.
* [x] add `appimage-integrate explain` and `run --detached`, and cover them with `tests/90-integration-conflicts.sh`.

* [x] document AppImage desktop-integration best practice in `documents/10-appimage-desktop-integration.md`.
* [x] document every desktop entry parameter in `documents/11-desktop-entry-parameters.md`.
* [x] document how to discover every loaded fact in `documents/12-desktop-loading-and-provenance.md`.
* [x] implement the icon theme locator (`sources/desktop/icon_theme_locator.*`) with a portable test.
* [x] implement the MIME association reader (`sources/desktop/mime_association_reader.*`), including the `mimeinfo.cache` fallback, with a portable test.
* [x] extend `desktop-inspect` with `--explain`, `--icon`, `--mime`, `--theme`, and `--why`.
* [x] implement the integration engine (`sources/integration/appimage_integrator.*`): plan, install, uninstall, list, and audit.
* [x] implement `appimage-integrate` with `plan`, `install`, `uninstall`, `list`, `run`, `audit`, `handler`, and `handle`.
* [x] add `make install` and `scripts/program-install.sh` to place the tools in `$HOME/.local/bin`.
* [x] add the sandbox tests `tests/integration_plan_test.cpp` and `tests/70-integration-sandbox.sh`.
* [x] add the provenance test `tests/80-desktop-provenance.sh`.
* [x] register `appimage-activator.desktop` as the real `*.AppImage` default, recording the previous defaults for restore.
* [x] preserve the AppImage MIME definitions in `$XDG_DATA_HOME/mime/packages/appimage.xml` before removing AppImageLauncher.
* [x] remove AppImageLauncher at the owner's request, and extend `audit` to report its leftover launchers and broken actions.
* [x] create the project from `00-project-skeleton` and record the baseline commit.
* [x] document the AppImage format in `documents/07-appimage-format.md`.
* [x] document the desktop entry format in `documents/08-desktop-entry-format.md`.
* [x] document the GNOME and XDG desktop file search paths in `documents/09-desktop-file-search-paths.md`.
* [x] implement the desktop entry reader and locator with portable unit tests.
* [x] implement the AppImage container reader and the SquashFS payload reader with portable and tool-gated tests.
* [x] implement `appimage-inspect` and the original `desktop-inspect` commands.
