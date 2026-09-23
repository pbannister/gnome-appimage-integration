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
| `appimage-integrate uninstall --identifier ID` | reverse one integration, restoring any replaced launcher |
| `appimage-integrate list` | list AppImages integrated by this tool |
| `appimage-integrate run <AppImage> [args]` | run once, forwarding arguments |
| `appimage-integrate run --detached <AppImage>` | start in a new session and report the process id |
| `appimage-integrate audit` | report every integration inconsistency on this desktop |
| `appimage-integrate handler status\|install\|uninstall` | manage the `*.AppImage` handler |
| `appimage-integrate handle <AppImage>` | the handler entry point: a GTK dialog, or the zenity fallback |
| `desktop-inspect <file.desktop>` | print one desktop entry |
| `desktop-inspect --explain ID` | show which file wins an identifier and what it masks |
| `desktop-inspect --icon NAME [--theme T] [--why]` | show where an icon resolves from |
| `desktop-inspect --mime TYPE [--why]` | show which application opens a type, and from where |

## Source Layout

- `sources/appimage/` contains the AppImage container reader and the SquashFS payload reader.
- `sources/desktop/` contains the desktop entry reader, the desktop entry locator, the icon theme locator, and the MIME association reader.
- `sources/integration/` contains the plan, install, uninstall, and audit engine.
- `sources/tools/` contains the command-line front ends and `appimage_handler_ui.py`, the GTK handler dialog.
- `sources/version/` contains the build-time version reporting.
- `sources/CMakeLists.txt` defines the targets; the build tree is written to `dataflow.out/build/`.

The graphical handler needs PyGObject with GTK4, which Ubuntu ships.
When it is absent, the handler falls back to zenity, and then to printing the equivalent commands.
The handler extracts the application version from `X-AppImage-Version`, then from AppStream metadata, then from the file name.
Every handler window remembers its size in `$XDG_DATA_HOME/gnome-appimage-integration/ui.json`.
A window position is remembered and restored only where the platform allows a client to choose it:
Wayland deliberately does not, so on a Wayland session the compositor places the window.
On X11, or under XWayland with `GDK_BACKEND=x11`, the handler restores the position with `xdotool` when it is installed, clamped so the window stays on the screen.
The `.desktop` file is never installed as an icon, and a replaced launcher is backed up rather than deleted.

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
