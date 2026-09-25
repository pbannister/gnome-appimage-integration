# Feature: AppImage Double-Click Handler

## Purpose

A double-clicked AppImage must do something useful instead of nothing. The project provides a
handler that asks the user what to do, and manages its own registration as the `*.AppImage`
default.

## Requirements

- `APPIMAGE-HANDLER-R001` — The project must provide a handler that accepts one AppImage path.
- `APPIMAGE-HANDLER-R002` — The handler must read the embedded application name for its prompt.
- `APPIMAGE-HANDLER-R003` — The handler must offer Run once, Integrate, Inspect, Cancel, and -- when the AppImage carries usable update information -- Update, which sits between Inspect and Close.
- `APPIMAGE-HANDLER-R004` — The handler must fall back to `zenity` when the graphical activator is not available and a graphical display exists.
- `APPIMAGE-HANDLER-R005` — The handler must fall back to printing the equivalent commands when no display is available.
- `APPIMAGE-HANDLER-R006` — Integrate must install the AppImage, report what was written, and offer to run it.
- `APPIMAGE-HANDLER-R007` — Inspect must show the embedded desktop entry.
- `APPIMAGE-HANDLER-R008` — Registering the handler must write a `NoDisplay=true` desktop entry whose `Exec` runs the installed tool with `%f`.
- `APPIMAGE-HANDLER-R009` — Registering the handler must set it as the default for `application/vnd.appimage`, `application/x-appimage`, and `application/x-iso9660-appimage`.
- `APPIMAGE-HANDLER-R010` — Registering the handler must record the previous default for each type before changing it.
- `APPIMAGE-HANDLER-R011` — Unregistering must restore the recorded previous defaults and remove the handler entry.
- `APPIMAGE-HANDLER-R012` — `handler status` must print the current default for each type and whether this tool owns it.
- `APPIMAGE-HANDLER-R013` — The handler must not require root.
- `APPIMAGE-HANDLER-R014` — Run once must start the AppImage in its own session and report "Starting <name> as process <pid>" as a notice that persists for several seconds.
- `APPIMAGE-HANDLER-R015` — Inspect must show a non-empty report containing the container facts and the embedded desktop entry.
- `APPIMAGE-HANDLER-R016` — Integrate must show the list of what was written.
- `APPIMAGE-HANDLER-R017` — Integrate must offer Replace existing, Add alongside, and Cancel when another launcher already represents the application.
- `APPIMAGE-HANDLER-R018` — The handler must prefer the graphical activator, a C++ GTK4 program, and fall back to zenity, then to printed instructions.
- `APPIMAGE-HANDLER-R019` — The activator must be built only when the GTK4 development files are present, so the command-line tool still builds without them.
- `APPIMAGE-HANDLER-R020` — The activator must not need Python at run time.
- `APPIMAGE-HANDLER-R021` — The main window must show the application Name, Comment, and GenericName, the detected version, and a details block containing File, Size, Integrate will, and Will install as.
- `APPIMAGE-HANDLER-R022` — The window must carry three logs in a tabbed panel, each a large read-only text box: Status, Discovered, and Actions, with Status shown by default.
- `APPIMAGE-HANDLER-R023` — The Status log must reflect the current state, including the already-installed notice and the conflict choice, and must not be repeated above the buttons.
- `APPIMAGE-HANDLER-R024` — The Discovered log must list the facts the state was deduced from, with the command that produced them, and must gain the full report when Inspect runs.
- `APPIMAGE-HANDLER-R025` — The Discovered log must emphasise the `This run` and `Error` fields, in the composed facts and in the appended report, and no other line.
- `APPIMAGE-HANDLER-R026` — The Actions log must record every command that changed the system, with a timestamp and the output that says what was done.
- `APPIMAGE-HANDLER-R027` — Each action must raise the page that answers it: Integrate the Status page, Inspect the Discovered page, and Run once or Run now the Actions page.
- `APPIMAGE-HANDLER-R028` — The first view's buttons must be ordered by how likely the owner is to use them: Integrate, Run once, Inspect, Update (when there is update information), Close.
- `APPIMAGE-HANDLER-R029` — Integration must check the `.sha256_sig` section: a hex digest is compared with the SHA-256 of the file with that section zeroed, and a PGP signature is checked with `gpg --verify` against the digest computed here.
- `APPIMAGE-HANDLER-R030` — A definite mismatch must refuse the install unless `--ignore-signature` is given, and must be visible in the activator's Status log; a missing gpg or an unknown key must be reported as not verified, never as a mismatch.
- `APPIMAGE-HANDLER-R031` — `explain --json` must report the newest installed version and how this file compares with it: `newer`, `older`, `same`, or `unknown`.
- `APPIMAGE-HANDLER-R032` — Close must be the suggested first-view action when the file is already integrated, and when it is older than the installed version.
- `APPIMAGE-HANDLER-R033` — An older file must be clearly visible in the Status log, with both versions, and integrating it anyway must suggest Add alongside and append its version to the name.
- `APPIMAGE-HANDLER-R034` — Add alongside must be suggested, with the version appended to the name, when more than one launcher already represents the application.
- `APPIMAGE-HANDLER-R035` — The most likely action must carry the GNOME HIG suggested-action style, and only one button in a view may carry it: Integrate on the first view, Run now after a successful integration, and Replace existing on the conflict choice, which is ordered Replace existing, Add alongside, Back.
- `APPIMAGE-HANDLER-R036` — A conflict must be described from the owner's point of view: the mode says "another launcher already represents this application", not that the run is blocked.
- `APPIMAGE-HANDLER-R037` — An AppImage that is in its managed directory, whose launcher runs it, and which has a record must be reported as `properly integrated`; the details row and the Status tab must say so and must not offer to replace what is already correct.
- `APPIMAGE-HANDLER-R038` — The main window must not show Type, Payload, or Embedded entry.
- `APPIMAGE-HANDLER-R039` — The handler must be a single window: the action buttons sit below the details block, and a large text area below the buttons takes the remaining height and is initially empty.
- `APPIMAGE-HANDLER-R040` — After Integrate succeeds, the window must follow the AppImage to its installed path, so File, Run now, and Inspect refer to the new location.
- `APPIMAGE-HANDLER-R041` — Update must ask the update information what is offered; when there is nothing newer to take it must say so in bold in the Status panel ("You already are using the latest version.") and disable the Update button, and otherwise download the offered file, keeping the previous one as `<name>.previous`.  The download must not freeze the window, and when it finishes the window must switch to the file that was installed, so Inspect, Integrate, and Run now refer to it.
- `APPIMAGE-HANDLER-R042` — The window must show the new location in its output after a move.
- `APPIMAGE-HANDLER-R043` — Inspect must write its details into that text area.
- `APPIMAGE-HANDLER-R044` — Integrate must write the integration result into that text area, and must list each existing launcher with its identifier, name, origin, version, target AppImage, icon, window class, and whether the target still exists.
- `APPIMAGE-HANDLER-R045` — The window must remember its size on resize and on close, and honour a remembered size even when it is larger than the reported work area; only the position is clamped on-screen.
- `APPIMAGE-HANDLER-R046` — The handler must set its program name to `appimage-activator`, and the entry must set `StartupWMClass=appimage-activator`, so the dock matches the running window to the entry and shows the activator icon.
- `APPIMAGE-HANDLER-R047` — When conflicts exist, the main window must add only "This application is already installed." and "Integrate will show details, then offer to replace or add alongside."
- `APPIMAGE-HANDLER-R048` — The window must be centred where the platform permits it, and must remember its position where the platform permits it; a restored geometry must be clamped so the window is entirely on the screen.
- `APPIMAGE-HANDLER-R049` — Position cannot be restored under Wayland, which does not let a client choose its own placement; the handler must not fail when that is the case.
- `APPIMAGE-HANDLER-R050` — The handler desktop entry must use the project's own AppImage Activator icon, installed into the user icon theme, and the AppImage MIME types must name that icon as their generic icon.
- `APPIMAGE-HANDLER-R051` — The right-click "Open With" item must be named `AppImage Activator`, because `AppImage Handler` is another project's name.
- `APPIMAGE-HANDLER-R052` — The entry, its record, and its icon must be `appimage-activator.desktop`, `appimage-activator.manifest`, and `appimage-activator.svg`.
- `APPIMAGE-HANDLER-R053` — `handler install` must migrate the pre-rename `appimage-handler.desktop`, `appimage-handler.svg`, and `appimage-handler.manifest` away, and carry their recorded previous defaults into the new record.
- `APPIMAGE-HANDLER-R054` — `handler status` must report a pre-rename entry that is still on disk and name the command that removes it.
- `APPIMAGE-HANDLER-R055` — After Integrate is clicked and a choice is offered, the window must show a `Name:` label and a single-line editable field to the left of the buttons, prefilled with the `Name` from the AppImage.
- `APPIMAGE-HANDLER-R056` — Add alongside and Replace existing must pass the field's value as `Name=` in the launcher, so several launchers for one application can carry their version or another hint.
- `APPIMAGE-HANDLER-R057` — The window's application id must equal the launcher's file name: the program name is `appimage-activator` and no `Gtk.Application` id is set, because GTK would otherwise send the application id as the Wayland app id and GNOME would show a generic dock icon.
- `APPIMAGE-HANDLER-R058` — `install --name NAME` must set `Name=` in the launcher while conflict detection keeps using the name from the embedded entry.
- `APPIMAGE-HANDLER-R059` — Every install must state which operation it performs: a new integration, an update in place, a repair of a moved AppImage, a replacement, or an add alongside.
- `APPIMAGE-HANDLER-R060` — The managed directory must be created when it is missing, and removing it must not make the tool mistake the AppImage for a different one: the identifier comes from the file's contents, so a moved AppImage is a repair.
- `APPIMAGE-HANDLER-R061` — `list` must mark a record whose AppImage is gone, and `audit` must report its missing `Exec` and `TryExec` targets.
- `APPIMAGE-HANDLER-R062` — `--replace` must work when the conflicting launcher sits at the target identifier, backing it up and restoring it on uninstall.
- `APPIMAGE-HANDLER-R063` — Output must not be duplicated by forked children: flush buffered output before forking.

## Behavior

- After `handler install`, `xdg-mime query default application/vnd.appimage` reports `appimage-activator.desktop`.
- After `handler uninstall`, the previously recorded handler is restored.
- With no display, `handle` prints the run, install, and inspect commands instead of failing.

## Dependencies

- `09-appimage-integration.md`
