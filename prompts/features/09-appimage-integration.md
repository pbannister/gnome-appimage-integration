# Feature: AppImage Integration

## Purpose

The project must turn a downloaded AppImage into an integrated desktop application, and must
make every step printable, plannable, and reversible.

## Requirements

- The integrator must build a plan that reads the AppImage and writes nothing.
- The plan must report the identifier, desktop file ID, installed path, launcher path, manifest path, icon name, `Exec`, field code, `StartupWMClass`, icons, MIME types, warnings, and actions.
- The identifier must be derived from the AppImage content, not from its name alone.
- The desktop file ID must come from the AppImage's embedded desktop entry name.
- The launcher must run the AppImage itself; the embedded `Exec`, which names an internal command, must not be copied.
- The launcher must quote the absolute AppImage path and append the embedded entry's own field code.
- The launcher must set `TryExec`, `Terminal=false`, and `StartupNotify=true`.
- The launcher must install icons under `hicolor/<size>/apps` and reference the icon by name.
- The integrator must prefer themed payload icons over root icons and must never write a `0x0` size directory.
- The integrator must remap a payload size directory that the icon theme does not list to a standard one.
- The integrator must refresh the icon theme cache after writing or removing icons, because GTK trusts a cache that is not older than the theme directory and then never rescans the directories.
- The integrator must derive the icon size from a payload theme directory, a PNG header, or the `scalable` convention.
- The integrator must record a manifest listing every file it wrote, so the install can be reversed.
- The integrator must refuse to overwrite a launcher it did not write unless the manifest exists.
- `install` must write an explicit plan and require confirmation unless `--yes` is given.
- `uninstall` must remove exactly the manifest's files and must optionally remove the AppImage.
- `list` must report the AppImages integrated by this tool.
- `audit` must report broken `Exec` and `TryExec` targets, broken context-action programs, unresolvable icons, absolute-path icons, invalid icon size directories, duplicate launchers for one application, AppImageLauncher provenance keys, and the current AppImage MIME defaults.
- `run` must make the AppImage executable, forward all arguments to it, and use `APPIMAGE_EXTRACT_AND_RUN=1` when FUSE is unavailable.
- The integrator must operate entirely under the user's home and must not require root.
- The integrator must detect existing launchers that already represent the application and report each one with its origin.
- The integrator must refuse an ambiguous install with a message that names the conflicting launchers and the `--replace` and `--add` choices.
- `--replace` must back up each displaced launcher and its manifest, and `uninstall` must restore them.
- `--add` must choose a distinct desktop file identifier instead of overwriting.
- The integrator must retire the manifest of a launcher it upgrades in place, so `list` stays accurate.
- The integrator must expose a human-readable description of the AppImage and of a plan.
- The integrator must extract the application version from `X-AppImage-Version`, then AppStream metadata, then the file name, and report which source it used.

## Behavior

- Planning a real type 2 AppImage reports the payload's own desktop entry name, icon sizes, and MIME package.
- Installing into a sandbox home writes only files under that home.
- Uninstalling leaves no launcher, icon, or manifest behind.
- Auditing this host reports the corrupt `Raspberry-Pi-Imager.desktop` and the AppImageLauncher leftovers.

## Dependencies

- `03-appimage-reader.md`
- `04-desktop-entry-reader.md`
- `05-desktop-entry-locator.md`
- `07-icon-theme-locator.md`
- `08-mime-association-reader.md`
