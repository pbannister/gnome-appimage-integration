# Feature: AppImage Integration

## Purpose

The project must turn a downloaded AppImage into an integrated desktop application, and must
make every step printable, plannable, and reversible.

## Requirements

- `APPIMAGE-INTEGRATION-R001` — The integrator must build a plan that reads the AppImage and writes nothing.
- `APPIMAGE-INTEGRATION-R002` — The plan must report the identifier, desktop file ID, installed path, launcher path, manifest path, icon name, `Exec`, field code, `StartupWMClass`, icons, MIME types, warnings, and actions.
- `APPIMAGE-INTEGRATION-R003` — The identifier must be derived from the AppImage content, not from its name alone.
- `APPIMAGE-INTEGRATION-R004` — The desktop file ID must come from the AppImage's embedded desktop entry name.
- `APPIMAGE-INTEGRATION-R005` — The launcher must run the AppImage itself; the embedded `Exec`, which names an internal command, must not be copied.
- `APPIMAGE-INTEGRATION-R006` — The launcher must quote the absolute AppImage path and append the embedded entry's own field code.
- `APPIMAGE-INTEGRATION-R007` — The launcher must set `TryExec`, `Terminal=false`, and `StartupNotify=true`.
- `APPIMAGE-INTEGRATION-R008` — The launcher must install icons under `hicolor/<size>/apps` and reference the icon by name.
- `APPIMAGE-INTEGRATION-R009` — The integrator must prefer themed payload icons over root icons and must never write a `0x0` size directory.
- `APPIMAGE-INTEGRATION-R010` — The integrator must remap a payload size directory that the icon theme does not list to a standard one.
- `APPIMAGE-INTEGRATION-R011` — The integrator must refresh the icon theme cache after writing or removing icons, because GTK trusts a cache that is not older than the theme directory and then never rescans the directories.
- `APPIMAGE-INTEGRATION-R012` — The integrator must derive the icon size from a payload theme directory, a PNG header, or the `scalable` convention.
- `APPIMAGE-INTEGRATION-R013` — The integrator must record a manifest listing every file it wrote, so the install can be reversed.
- `APPIMAGE-INTEGRATION-R014` — The integrator must refuse to overwrite a launcher it did not write unless the manifest exists.
- `APPIMAGE-INTEGRATION-R015` — `install` must write an explicit plan and require confirmation unless `--yes` is given.
- `APPIMAGE-INTEGRATION-R016` — `uninstall` must remove exactly the manifest's files and must optionally remove the AppImage.
- `APPIMAGE-INTEGRATION-R017` — `list` must report the AppImages integrated by this tool.
- `APPIMAGE-INTEGRATION-R018` — `audit` must report broken `Exec` and `TryExec` targets, broken context-action programs, unresolvable icons, absolute-path icons, invalid icon size directories, duplicate launchers for one application, AppImageLauncher provenance keys, and the current AppImage MIME defaults.
- `APPIMAGE-INTEGRATION-R019` — `run` must make the AppImage executable, forward all arguments to it, and use `APPIMAGE_EXTRACT_AND_RUN=1` when FUSE is unavailable.
- `APPIMAGE-INTEGRATION-R020` — The integrator must operate entirely under the user's home and must not require root.
- `APPIMAGE-INTEGRATION-R021` — The integrator must detect existing launchers that already represent the application and report each one with its origin.
- `APPIMAGE-INTEGRATION-R022` — The integrator must refuse an ambiguous install with a message that names the conflicting launchers and the `--replace` and `--add` choices.
- `APPIMAGE-INTEGRATION-R023` — `--replace` must back up each displaced launcher and its manifest, and `uninstall` must restore them.
- `APPIMAGE-INTEGRATION-R024` — `--add` must choose a distinct desktop file identifier instead of overwriting.
- `APPIMAGE-INTEGRATION-R025` — The integrator must retire the manifest of a launcher it upgrades in place, so `list` stays accurate.
- `APPIMAGE-INTEGRATION-R026` — The integrator must expose a human-readable description of the AppImage and of a plan.
- `APPIMAGE-INTEGRATION-R027` — The integrator must recover a missing `StartupWMClass` from a previous manifest for the same AppImage, from a conflicting launcher, or from a launcher this tool displaced, and must persist the chosen class in the manifest.
- `APPIMAGE-INTEGRATION-R028` — A class set with `--wm-class`, or by an earlier install that used it, must be recorded as an explicit override in the manifest and must outrank the embedded entry on later installs.
- `APPIMAGE-INTEGRATION-R029` — A class the launcher already carries must outrank the embedded entry, because the embedded value is the AppImage author's guess; a launcher that lost the class must not let it win over the manifest's remembered choice.
- `APPIMAGE-INTEGRATION-R030` — The update information must be understood without a network: the transport, the repository, the release and the file-name pattern, resolved into the URL a check would fetch, with a value that is absent, unknown, or not implementable named as such rather than treated as an error.
- `APPIMAGE-INTEGRATION-R031` — A check must ask the named transport what it has and compare it with the installed version, without downloading anything; it must say when it cannot tell (a zsync file names a file, not a version), and it must not depend on the AppImage being integrated.
- `APPIMAGE-INTEGRATION-R032` — The launcher must offer an Update context item only when the AppImage carries usable update information, and that item must open the activator as if its Update button had been clicked rather than running the update behind the window's back.
- `APPIMAGE-INTEGRATION-R033` — An update must use a copy of the offered file that is already beside the installed one instead of downloading it again, and must refuse an unusable copy rather than replace a working file with it.
- `APPIMAGE-INTEGRATION-R034` — An update must download the offered file under its own name beside the installed one, point every record and launcher that ran the replaced file at the new one, and keep the replaced file as `<name>.previous` unless `--no-backup` is given.
- `APPIMAGE-INTEGRATION-R035` — An update must download to a temporary file beside the installed one, verify it (the published digest, the file's own signature, and that it is a readable type 2 AppImage), and only then replace the file and re-render that one launcher; a mismatch or an unreadable download must leave the working file untouched and remove the temporary one.
- `APPIMAGE-INTEGRATION-R036` — An update must never act when the check could not be made, must say when nothing was published to verify against, and must require `--force` when the transport cannot show that the offered file is newer.
- `APPIMAGE-INTEGRATION-R037` — `migrate` must adopt a launcher another tool wrote by taking the AppImage from its `Exec`, installing this tool's launcher for it, and having the displaced launcher backed up so `uninstall` restores it.
- `APPIMAGE-INTEGRATION-R038` — `audit` must report a recorded AppImage whose update information is missing or unusable, and `audit --check` must report which ones have an update waiting.
- `APPIMAGE-INTEGRATION-R039` — `refresh` must re-render every recorded launcher from its embedded entry, preserve the `Name=` the launcher carries, its desktop id, its icon name and its window class, keep each record's own identifier, and name any record whose AppImage is gone instead of failing silently.
- `APPIMAGE-INTEGRATION-R040` — The tool must be able to read the class from the running application: `windows` must list the running AppImages with the class each one reports (the program inside the mount, or an X11 client's `WM_CLASS`), and `--wm-class-from-window` must apply it or refuse with the candidates named.
- `APPIMAGE-INTEGRATION-R041` — Re-running install for the same AppImage must be harmless: it must overwrite its own launcher, icons, and manifest and must never create a duplicate, so it can repair a faulty install.
- `APPIMAGE-INTEGRATION-R042` — The integrator must extract the application version from `X-AppImage-Version`, then AppStream metadata, then the file name, and report which source it used.

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
