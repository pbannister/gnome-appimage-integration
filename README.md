# GNOME AppImage Integration

This project reads AppImage files and `*.desktop` files so that an AppImage can be
integrated into the GNOME application menu.

The project is derived from `00-project-skeleton`.
The skeleton's interaction rules, workflow, and conventions apply unchanged.

## For the LLM

The LLM should begin by reading these files in this order (the numeric prefix marks the load order):

1. `prompts/01-contract.md`
2. `prompts/02-workflow.md`
3. `prompts/03-conventions.md`

- These define the interaction rules, workflow, and formatting conventions.
- The LLM must follow the workflow defined in `prompts/02-workflow.md` for every task.
- This project targets C++, so `prompts/flavors/02-cpp-conventions.md` applies to source work.

## For Human Contributors

Human contributors should begin by reading:

- `prompts/README.md`
- `documents/README.md`

## Project Scope

The project reads two file formats, locates one of them on disk, and integrates AppImages into the desktop.

1. Read an AppImage container: image type, ELF facts, payload offset and size, embedded update information and signature, and the SquashFS payload.
2. Read a desktop entry file: groups, keys, values, localized values, lists, escapes, actions, and `Exec` field codes.
3. Locate desktop entry files using the GNOME (GIO) search path built from `$XDG_DATA_HOME` and `$XDG_DATA_DIRS`.
4. Resolve icon names through the freedesktop icon theme, and MIME defaults through `mimeapps.list` and `mimeinfo.cache`, so every loaded fact has a named source.
5. Plan, install, and reverse the integration of one AppImage: managed location, launcher, hicolor icons, and a manifest.
6. Handle a double-clicked `*.AppImage` and manage the registration of that handler.

The project does not modify an AppImage; it moves or copies it, and it never requires root.
`appimage-integrate run` executes the AppImage only when the user asks for it.

## Research Findings

The format research that the readers implement is recorded in `documents/`.

- `documents/07-appimage-format.md` — the AppImage specification, the payload offset algorithm, and the SquashFS superblock.
- `documents/08-desktop-entry-format.md` — the Desktop Entry Specification version 1.5.
- `documents/09-desktop-file-search-paths.md` — where GNOME looks for `*.desktop` files, with the observed host values.
- `documents/10-appimage-desktop-integration.md` — best practice for using and integrating an AppImage, and what integration writes.
- `documents/11-desktop-entry-parameters.md` — every desktop entry parameter, what it does, and how to inspect it.
- `documents/12-desktop-loading-and-provenance.md` — how to discover where applications, icons, and parameters are loaded from.

## Top-Level Map

- `README.md` is the project overview.
- `TODO.md` tracks pending and completed project tasks.
- `PHASES.md` records the current phase and its milestones.
- `prompts/` contains LLM interaction rules, common requirements, feature requirements, task definitions, and episode work orders.
- `documents/` contains human-consumption documents: the interaction pattern, worked examples, tool notes, and the format research.
- `records/` contains version-controlled outcome, incident, and handoff records.
- `tools/` contains tool-specific rules.
- `tools/aider-rules.md` is used only with Aider.
- `sources/` contains the C++ readers and command-line tools.
- `scripts/` contains project scripts.
- `tests/` contains tests and validation code.
- `dataflow.in/` contains input data, including sample desktop entry files.
- `dataflow.out/` contains generated data output and the build tree (not version-controlled).
- `logs/` contains generated logs (not version-controlled).
- `site.in/` contains static-site input.
- `site.out/` contains generated static-site output (not version-controlled).
- `Makefile` drives the build (`make build`), the tests (`make test`), the pages (`make site`), and cleanup (`make clean`).

## Installing

A release carries the built tools for Linux, and one script installs them:

```sh
curl -fsSL https://raw.githubusercontent.com/pbannister/gnome-appimage-integration/master/scripts/install.sh | sh
```

The script fetches the release tarball for this machine, checks it against the release's
`SHA256SUMS`, and installs into `$HOME/.local`. `PREFIX` installs elsewhere, `VERSION` pins a
release tag, and where no prebuilt build exists for the machine it builds from the source archive
instead. It installs the tools only: making this the `*.AppImage` handler stays a deliberate step,
and the script prints it:

```sh
appimage-integrate handler install
```

`scripts/release-package.sh` makes the assets a release publishes, and `make release-publish` (with
the GitHub CLI authenticated) tags the commit and publishes them.

## Building and Testing

- `make build` configures and compiles the readers and command-line tools into `dataflow.out/build/`.
- `make install` installs `appimage-inspect`, `desktop-inspect`, and `appimage-integrate` into `$HOME/.local/bin` (`PREFIX` overrides).
- `make test` runs every test in `tests/` and writes a timestamped log to `logs/`.
- `make site` builds the published page set.
- `make release` builds the release assets into `dataflow.out/release/`.
- `make release-publish` tags the commit and publishes a GitHub release (needs `gh`, authenticated).
- `make clean` removes generated output.

The C++ build uses CMake with the highest warning level and treats warnings as errors.

## Commands

| Command | Purpose |
| ------- | ------- |
| `appimage-inspect <AppImage>` | container facts, payload listing, and the embedded desktop entry |
| `appimage-integrate explain <AppImage>` | a full human-readable report, including what install would write |
| `appimage-integrate plan <AppImage>` | print exactly what integration would write; change nothing |
| `appimage-integrate install <AppImage>` | integrate the AppImage, after confirmation |
| `appimage-integrate install --replace <AppImage>` | replace an existing launcher, backing it up |
| `appimage-integrate install --add <AppImage>` | install alongside an existing launcher |
| `appimage-integrate install --wm-class CLASS <AppImage>` | set the window class the dock matches on |
| `appimage-inspect --update-url <AppImage>` | resolve the update information into the URL and asset pattern it names |
| `appimage-integrate windows` | list running AppImages and the window class each one reports |
| `appimage-integrate install --wm-class-from-window <AppImage>` | read that class from the running application |
| `appimage-integrate install --name NAME <AppImage>` | set `Name=` in the launcher, e.g. to carry a version |
| `appimage-integrate install --install-dir DIR <AppImage>` | use another managed directory |
| `appimage-integrate migrate <launcher>` | adopt a launcher another tool wrote for an AppImage |
| `appimage-integrate uninstall --identifier ID` | reverse one integration, restoring any replaced launcher |
| `appimage-integrate list` | list AppImages integrated by this tool, marking any whose file is gone |
| `appimage-integrate refresh` | rewrite every recorded launcher from its embedded entry, keeping names, ids, icons and window classes |
| `appimage-integrate refresh --dry-run` | show what refresh would rewrite, and change nothing |
| `appimage-integrate run <AppImage> [args]` | run once, forwarding arguments |
| `appimage-integrate run --detached <AppImage>` | start in a new session and report the process id |
| `appimage-integrate update --check <AppImage>` | ask the embedded update information whether a newer build exists |
| `appimage-integrate update --check --all` | do that for every AppImage this tool integrated |
| `appimage-integrate update <AppImage>` | replace the file with what the transport offers, after verifying the download |
| `appimage-integrate update --all --yes` | do that for every recorded AppImage that has an update waiting |
| `appimage-integrate update --force <AppImage>` | take the offered file when no version can be compared |
| `appimage-integrate update --no-backup <AppImage>` | remove the replaced file instead of keeping it as `<name>.previous` |
| `appimage-integrate audit [--check]` | report every integration inconsistency on this desktop, and with `--check` which AppImages have an update waiting |
| `appimage-integrate install --ignore-signature <AppImage>` | integrate although `.sha256_sig` does not match the payload |

An AppImage whose `.sha256_sig` section holds a digest or a signature is checked: the digest covers the file with that section zeroed. `explain`, `plan`, `appimage-inspect` and `install` report the result, a mismatch refuses the install unless `--ignore-signature` is given, and the activator shows it in Status.

`refresh` is for launchers written by an older version of this tool: it re-renders each one from the AppImage's embedded entry, so new keys appear in one command, and preserves what the embedded entry does not carry (the `Name=` you chose, the desktop id, the icon name, the window class, and the launcher's own record). `--wm-class CLASS` uses that class for every launcher it rewrites; `--wm-class-from-window` reads the class from each running application instead. A recorded AppImage that is no longer at its path is named and skipped, and makes the command exit non-zero.

`update --check` reads the AppImage's `.upd_info` section and asks the transport it names what it has, then compares that with the installed version: `gh-releases-zsync` through the GitHub API (release, prerelease or a specific tag, with the asset matched by the specification's file-name pattern), and `zsync` by reading the `Filename:` header of the zsync file. It downloads nothing. A value that is absent, not a transport the specification defines (Cura's `guess` is an `appimagetool` option), or one this tool cannot follow (`pling-v1-zsync`, `bintray-zsync`) is reported as such, and makes the command exit non-zero. `appimage-inspect --update-url` resolves the same field offline, so the pipe-separated string can be read without a network.

`update` without `--check` downloads the file the transport offers under its own name, beside the file it replaces. It downloads to `<name>.part`, verifies the download, and only then does anything: the new file becomes the managed one, every launcher and record that ran the old file is re-rendered against it (desktop id, icon, `Name=` and window class kept), and the old file is kept as `<name>.previous` unless `--no-backup` is given. The window's **Update** button and the launcher's **Update** item do the same thing, and the window switches to the new file when the download finishes. When there is nothing newer to take, Status says so in bold — *You already are using the latest version.* — and the Update button is disabled. If the offered file is already beside the installed one — an AppImage downloaded by hand, or a build integrated earlier — it is verified and used, and nothing is downloaded. Verification compares the release's published `<asset>-SHA256.txt` when there is one and always checks the downloaded file's own `.sha256_sig`; a mismatch refuses the update and leaves the working file untouched. A download with nothing published to check against is reported as exactly that. `--force` takes the offered file when the check cannot show it is newer (a zsync file names no version), and `--dry-run` prints the download URL and size without fetching anything.

The launcher each integration writes carries **AppImage Activator**, which opens the graphical activator on that file, and **Remove this AppImage**. It also carries **Update**, which opens the activator as if its Update button had been clicked — but only when the AppImage carries usable update information. GNOME Shell also adds its own **App Details** item, which opens GNOME Software; that item belongs to the shell and cannot be suppressed from a desktop entry.
| `appimage-integrate handler status\|install\|uninstall` | manage the `*.AppImage` handler (the AppImage Activator) |
| `appimage-integrate handle <AppImage>` | the handler entry point: a GTK dialog, or the zenity fallback |
| `desktop-inspect <file.desktop>` | print one desktop entry |
| `desktop-inspect --explain ID` | show which file wins an identifier and what it masks |
| `desktop-inspect --icon NAME [--theme T] [--why]` | show where an icon resolves from |
| `desktop-inspect --mime TYPE [--why]` | show which application opens a type, and from where |

## Source Layout

- `sources/appimage/` contains the AppImage container reader and the SquashFS payload reader.
- `sources/desktop/` contains the desktop entry reader, the desktop entry locator, the icon theme locator, and the MIME association reader.
- `sources/integration/` contains the plan, install, uninstall, and audit engine.
- `sources/tools/` contains the command-line front ends and `appimage_activator_ui.cpp`, the GTK4 activator program.
- `sources/json/` contains the minimal JSON value the activator reads `explain --json` and its geometry file with.
- `sources/version/` contains the build-time version reporting.
- `sources/CMakeLists.txt` defines the targets; the build tree is written to `dataflow.out/build/`.

The graphical activator is C++ with GTK4. The build needs the GTK4 development files (`libgtk-4-dev` or `gtk4-devel`); without them the activator is not built, and the handler falls back to zenity, and then to printing the equivalent commands.
`appimage-activator --activate ACTION[,ACTION...]` and `--set-name TEXT` drive the window without a person, which is how `tests/95-activator-ui.sh` exercises it under Xvfb; the desktop entry never passes them.
It is a single window: the application name, version, generic name, and comment, then File, Size, Integrate will, and Will install as, then the Name field and the action buttons, then a tabbed panel of three text logs. `Integrate will` and Status both read the tool's mode, so an AppImage that is already complete says `properly integrated` and nothing more to do. **Status** is the page shown first and holds the current state and any decision the window is waiting for; **Discovered** holds the evidence the state was deduced from, plus the raw report whenever Inspect runs, with `This run` and `Error` emphasised because they decide what happens; **Actions** is a timestamped log of every command that changed the system, with its output. Each action raises the page that answers it: Integrate the resulting Status, Inspect the Discovered report, Run once the Actions log. Buttons are ordered by how likely the owner is to use them — Integrate, Run once, Inspect, Close — and the likely one carries the GNOME HIG's suggested-action style (Integrate, then Run now after a successful integration, and Replace existing on the conflict choice).
The handler extracts the application version from `X-AppImage-Version`, then from AppStream metadata, then from the file name.
`handler install` gives the handler its own AppImage Activator icon, installs it into the user icon theme, and points the AppImage MIME types at it, so AppImage files and the right-click item share one icon.
It sets its program name to `appimage-activator` and the entry sets `StartupWMClass=appimage-activator`, so the dock matches the running window to the entry instead of showing a generic icon.
The window's application id is its program name, `appimage-activator`, because no `Gtk.Application` id is set: GTK uses the application id when there is one and the program name otherwise, and GNOME only matches a Wayland window to a launcher whose file name is that same id.
The right-click "Open With" item is named `AppImage Activator`, because `AppImage Handler` is another project's name; `handler install` also removes the pre-rename `appimage-handler` entry, icon, and record, and carries their recorded previous defaults into the new record.
It remembers its size in `$XDG_DATA_HOME/gnome-appimage-integration/ui.json` and honours it on the next run; the remembered position is clamped so the window stays on the screen.

## Window Placement

A client cannot choose its own position under Wayland, and GNOME's `org.gnome.mutter center-new-windows` is off by default, so the handler opens where the compositor puts it.
The practical choices are:

| Choice | How | Trade-off |
| ------ | --- | --------- |
| Let the compositor place it | do nothing | no control over position |
| Centre every new window | `gsettings set org.gnome.mutter center-new-windows true` | a session-wide setting, not specific to this handler |
| Position it from the client | install `xdotool` and run the handler under XWayland with `GDK_BACKEND=x11` | position and centring then work, at the cost of XWayland |
| Use a window-placement extension | install a GNOME Shell extension that places windows by app id | another component to maintain |
| Anchor it as an overlay | a layer-shell client | GNOME does not implement `wlr-layer-shell`, so this is not available on GNOME |

Position is saved and restored only through the XWayland path; on a Wayland session the handler still remembers and restores the size.
The `.desktop` file is never installed as an icon, and a replaced launcher is backed up rather than deleted.
Every icon write refreshes the icon theme cache with `gtk4-update-icon-cache`, because GTK trusts a cache that is not older than the theme directory and then never rescans it — a stale cache hides every newly installed icon.
Re-running `install` for the same AppImage is harmless: it overwrites its own launcher, icons, and manifest, so it also repairs a faulty install. A missing `StartupWMClass` is recovered from a previous manifest, from a conflicting launcher, or from one this tool displaced, and the chosen class is remembered. Use `--wm-class` when the embedded entry does not name the class the dock matches on; a class chosen that way is recorded as an explicit override and survives later re-integrations that the embedded entry would otherwise win back. `windows` and `--wm-class-from-window` read the class from the running application instead: from the program inside the AppImage's mount, or from an X11 window's `WM_CLASS`. GNOME does not let another program list windows, so a native Wayland window has to be read in Looking Glass; `windows` says so when it finds nothing.

A *different* AppImage that wants an identifier this tool already owns is never a silent takeover: `explain` and `plan` name it, and `install` refuses until `--replace` (the displaced launcher is backed up and restored by `uninstall`) or `--add` (the existing launcher, its record, and its icon are kept, and this AppImage is installed alongside as `<id>-2.desktop` with the record `<identifier>-2`) is chosen. `audit` reports any launcher whose record disagrees with the AppImage it actually runs.

## Project Pages (publishing conventions)

- A project derived from the skeleton publishes the standard page set
  (status, dashboard, condensed todo/prompts/documents) per
  `prompts/features/02-project-pages.md` and
  `documents/06-project-pages.md`.
- `make site` builds the full set (`scripts/site-build.sh` +
  `scripts/site-condense.sh`).
- Publishing goes through the homelab project (homelab-publish): the project
  registers once via `pages_source` and does not push to the web server
  itself — `make deploy` is retired.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `README.md`
- `TODO.md`
- `Makefile`
