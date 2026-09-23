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

## Building and Testing

- `make build` configures and compiles the readers and command-line tools into `dataflow.out/build/`.
- `make install` installs `appimage-inspect`, `desktop-inspect`, and `appimage-integrate` into `$HOME/.local/bin` (`PREFIX` overrides).
- `make test` runs every test in `tests/` and writes a timestamped log to `logs/`.
- `make site` builds the published page set.
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
| `appimage-integrate install --name NAME <AppImage>` | set `Name=` in the launcher, e.g. to carry a version |
| `appimage-integrate install --install-dir DIR <AppImage>` | use another managed directory |
| `appimage-integrate uninstall --identifier ID` | reverse one integration, restoring any replaced launcher |
| `appimage-integrate list` | list AppImages integrated by this tool, marking any whose file is gone |
| `appimage-integrate run <AppImage> [args]` | run once, forwarding arguments |
| `appimage-integrate run --detached <AppImage>` | start in a new session and report the process id |
| `appimage-integrate audit` | report every integration inconsistency on this desktop |
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
It is a single window: the application name, version, generic name, and comment, then File, Size, Integrate will, and Will install as, then the Name field and the action buttons, then a tabbed panel of three text logs. `Integrate will` and Status both read the tool's mode, so an AppImage that is already complete says `properly integrated` and nothing more to do. **Status** is the page shown first and holds the current state and any decision the window is waiting for; **Discovered** holds the evidence the state was deduced from, plus the raw report whenever Inspect runs; **Actions** is a timestamped log of every command that changed the system, with its output.
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
Re-running `install` for the same AppImage is harmless: it overwrites its own launcher, icons, and manifest, so it also repairs a faulty install. A missing `StartupWMClass` is recovered from a previous manifest, from a conflicting launcher, or from one this tool displaced, and the chosen class is remembered. Use `--wm-class` when the embedded entry does not name the class the dock matches on.

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
