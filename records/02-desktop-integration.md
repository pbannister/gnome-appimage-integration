# Record: desktop integration

## Outcome

Phase 2 made AppImage desktop integration concrete and overt.
The recommended best practice is documented, and every fact the desktop loads can now be
traced to its source with a command.

The project now plans, installs, uninstalls, runs, and audits AppImage integration, resolves
icon names and MIME defaults with their provenance, and handles a double-clicked AppImage.
AppImageLauncher was removed at the owner's request, and its AppImage MIME definitions were
preserved so the new handler keeps working.

## What Was Built

| Area | Files |
| ---- | ----- |
| Icon theme locator | `sources/desktop/icon_theme_locator.h`, `sources/desktop/icon_theme_locator.cpp` |
| MIME association reader | `sources/desktop/mime_association_reader.h`, `sources/desktop/mime_association_reader.cpp` |
| Integration engine | `sources/integration/appimage_integrator.h`, `sources/integration/appimage_integrator.cpp` |
| Integration CLI | `sources/tools/appimage_integrate.cpp` |
| Provenance CLI | `sources/tools/desktop_inspect.cpp` (`--explain`, `--icon`, `--mime`, `--theme`, `--why`) |
| Install path | `scripts/program-install.sh`, `make install` |
| Research | `documents/10-appimage-desktop-integration.md`, `documents/11-desktop-entry-parameters.md`, `documents/12-desktop-loading-and-provenance.md` |

## Decisions

- The user chose to register this project's handler as the real `*.AppImage` default, replacing AppImageLauncher's role, and to remove AppImageLauncher.
- The handler is `appimage-integrate handle %f`; it offers Run once, Integrate, Inspect, and Cancel through `zenity`, with a command-printing fallback when no display exists.
- Registration records the previous default for each MIME type, so `handler uninstall` restores it.
- The AppImage MIME definitions were copied from the AppImageLauncher package into `$XDG_DATA_HOME/mime/packages/appimage.xml` before removal, because the package owned them and removing it would otherwise delete the types.
- The desktop file ID comes from the AppImage's own embedded desktop entry name, for example `org.openshot.OpenShot.desktop`.
- The launcher runs the AppImage itself; the embedded `Exec`, which names an internal command such as `AppRun` or `openshot-qt-launch`, is never copied.
- The icon name is `appimage_<hash8>_<name>`, so an installed icon cannot silently shadow a system icon.
- Icons are installed under `hicolor/<size>/apps` and referenced by name, and the size is taken from the payload theme directory, a PNG header, or the `scalable` convention, so `0x0` is never written.
- The installed tools live in `$HOME/.local/bin`, so the launcher's `Remove` action has a stable target.
- Every install writes a manifest under `$XDG_DATA_HOME/gnome-appimage-integration/`, which is what makes the install reversible.
- `audit` reports broken `Exec`, `TryExec`, and context-action targets, unresolvable or absolute-path icons, invalid icon size directories, duplicate launchers, AppImageLauncher provenance, and the current MIME defaults.
- The MIME reader falls back to `mimeinfo.cache`, because that is what `xdg-mime` and the desktop actually use when no default is recorded in `mimeapps.list`.

## The AppImageLauncher Removal

This was a change to the owner's desktop, requested by the owner.

- Fallback staged first: the AppImage MIME definitions were copied to `$XDG_DATA_HOME/mime/packages/appimage.xml` and `update-mime-database` was run, and the previous MIME defaults were already recorded in the handler manifest.
- The package was `appimagelauncher` 3.0.0-beta-2, owned by dpkg, and root-owned.
- It was removed with `apt-get remove`, then `apt-get purge`.
- Verified after removal: the package is gone, `/usr/share/applications/appimagelauncher.desktop`, `/opt/appimagelauncher.AppDir`, `/usr/share/mime/packages/appimage.xml`, and its icon are gone, `xdg-mime query filetype` on an AppImage still reports `application/vnd.appimage`, and the default handler for all three AppImage MIME types is `appimage-handler.desktop`.
- Left in place deliberately: the `appimagekit_*` launchers and icons AppImageLauncher wrote, and the application-specific MIME packages under `$XDG_DATA_HOME/mime/packages/appimagekit_*.xml`. The launchers still start their applications; `audit` reports their broken `/opt/appimagelauncher.AppDir/...` context actions.

## Verification

Verified 2026-09-21 with `make test` from the repository root; all thirteen test scripts passed.

| Test | Tier | Result |
| ---- | ---- | ------ |
| `tests/00-skeleton.sh`, `01`, `02` | portable | pass |
| `tests/10-desktop-entry-reader.sh` | portable | pass |
| `tests/12-icon-theme-locator.sh` | portable | pass |
| `tests/20-desktop-entry-locator.sh` | portable | pass |
| `tests/22-mime-association-reader.sh` | portable | pass |
| `tests/30-appimage-reader.sh` | portable | pass |
| `tests/40-squashfs-payload-reader.sh` | tool-gated | pass for gzip, xz, zstd, uncompressed |
| `tests/50-desktop-search-path-live.sh` | live-state | PASS, 5 directories, 187 entries |
| `tests/60-command-line-tools.sh` | integration | pass |
| `tests/70-integration-sandbox.sh` | tool-gated | pass |
| `tests/80-desktop-provenance.sh` | portable | pass |

Additional evidence:

- `desktop-inspect --mime application/vnd.appimage` now reports `appimage-handler.desktop` from `$XDG_CONFIG_HOME/mimeapps.list`, matching `xdg-mime query default`.
- `appimage-integrate plan` on the real OpenShot AppImage reports the embedded `org.openshot.OpenShot.desktop`, five themed icon sizes, and the `%F` field code, and warns that the entry has no `StartupWMClass`.
- `appimage-integrate run` was verified against a synthetic AppImage whose ELF part is `/bin/true`, with arguments forwarded and no stderr output.
- `appimage-integrate audit` reports the corrupt `Raspberry-Pi-Imager.desktop`, the two `;Icon`/`;Exec` parse failures, the `0x0` icon directory, and the AppImageLauncher leftovers.

## Commits

- `965f35a` feat: integrate AppImages and make icons, MIME, and parameters overt

## Next Tasks

1. Decide whether to migrate the five AppImageLauncher-created launchers to this tool, which would replace their broken context actions.
2. Consider a `migrate` subcommand for that replacement.
3. Decide how to discover `StartupWMClass` on Wayland.
