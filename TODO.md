# TODO

## Open Questions

* [ ] decide whether to migrate the five AppImageLauncher-created `appimagekit_*` launchers to this tool.
      — Migration removes their now-broken `Remove`/`Update` actions, but replaces launchers the owner already uses.
* [ ] decide how to set `StartupWMClass` automatically. `xprop WM_CLASS` works on X11; a Wayland application id needs the compositor.
* [ ] decide whether `run` should retry with `APPIMAGE_EXTRACT_AND_RUN=1` after a FUSE failure at runtime, not only when FUSE is absent.
* [ ] decide whether integration should verify the `.sha256_sig` signature when the section is non-empty.
* [ ] decide the icon naming policy: this tool writes `appimage_<hash8>_<name>`; AppImageLauncher wrote `appimagekit_<md5>_<name>`.
* [ ] decide how `--add` should distinguish two launchers sharing one `Name` in the menu; suffixing the Name with a version is the obvious option.
* [ ] decide whether the zenity fallback should persist a window size at all; zenity cannot report a dragged size, so only the GTK handler can remember one.
* [ ] decide whether the handler should set `org.gnome.mutter center-new-windows true` on install; it is a session-wide setting, so it is left to the owner.
* [ ] consider offering a one-command way to run the handler under XWayland with `xdotool` for owners who want saved window positions.
* [ ] improve the human-oriented documents in `documents/`.
* [ ] read the tool-universe sources in `documents/02-tool-universe.md`.

## Pending

* [ ] migrate the existing AppImageLauncher launchers, if the owner wants them replaced.
* [ ] consider a `migrate` subcommand that installs this tool's launcher and removes a named older one.
* [ ] add a `--startup-wm-class-from-window` helper that reads `xprop` for the next window that appears.

## Recently Completed

* [x] add a GTK4 handler dialog (`sources/tools/appimage_handler_ui.py`) that remembers its window size between runs.
* [x] show Name, Comment, GenericName, and the detected version in the handler's first window.
* [x] make Inspect open a separate window with only a Close button, returning to the AppImage window.
* [x] make the Integrate dialog list each existing launcher's id, name, origin, version, target, icon, window class, and whether the target exists.
* [x] extract the application version from `X-AppImage-Version`, then AppStream, then the file name, and report the source.
* [x] add `explain --json` for the graphical handler, and `tests/95-handler-ui.sh` for the script and its dispatch.
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
* [x] register `appimage-handler.desktop` as the real `*.AppImage` default, recording the previous defaults for restore.
* [x] preserve the AppImage MIME definitions in `$XDG_DATA_HOME/mime/packages/appimage.xml` before removing AppImageLauncher.
* [x] remove AppImageLauncher at the owner's request, and extend `audit` to report its leftover launchers and broken actions.
* [x] create the project from `00-project-skeleton` and record the baseline commit.
* [x] document the AppImage format in `documents/07-appimage-format.md`.
* [x] document the desktop entry format in `documents/08-desktop-entry-format.md`.
* [x] document the GNOME and XDG desktop file search paths in `documents/09-desktop-file-search-paths.md`.
* [x] implement the desktop entry reader and locator with portable unit tests.
* [x] implement the AppImage container reader and the SquashFS payload reader with portable and tool-gated tests.
* [x] implement `appimage-inspect` and the original `desktop-inspect` commands.
