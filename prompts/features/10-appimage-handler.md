# Feature: AppImage Double-Click Handler

## Purpose

A double-clicked AppImage must do something useful instead of nothing. The project provides a
handler that asks the user what to do, and manages its own registration as the `*.AppImage`
default.

## Requirements

- The project must provide a handler that accepts one AppImage path.
- The handler must read the embedded application name for its prompt.
- The handler must offer Run once, Integrate, Inspect, and Cancel.
- The handler must fall back to `zenity` when the graphical activator is not available and a graphical display exists.
- The handler must fall back to printing the equivalent commands when no display is available.
- Integrate must install the AppImage, report what was written, and offer to run it.
- Inspect must show the embedded desktop entry.
- Registering the handler must write a `NoDisplay=true` desktop entry whose `Exec` runs the installed tool with `%f`.
- Registering the handler must set it as the default for `application/vnd.appimage`, `application/x-appimage`, and `application/x-iso9660-appimage`.
- Registering the handler must record the previous default for each type before changing it.
- Unregistering must restore the recorded previous defaults and remove the handler entry.
- `handler status` must print the current default for each type and whether this tool owns it.
- The handler must not require root.
- Run once must start the AppImage in its own session and report "Starting <name> as process <pid>" as a notice that persists for several seconds.
- Inspect must show a non-empty report containing the container facts and the embedded desktop entry.
- Integrate must show the list of what was written.
- Integrate must offer Replace existing, Add alongside, and Cancel when another launcher already represents the application.
- The handler must prefer the graphical activator, a C++ GTK4 program, and fall back to zenity, then to printed instructions.
- The activator must be built only when the GTK4 development files are present, so the command-line tool still builds without them.
- The activator must not need Python at run time.
- The main window must show the application Name, Comment, and GenericName, the detected version, and a details block containing File, Size, Integrate will, and Will install as.
- The window must carry three logs in a tabbed panel, each a large read-only text box: Status, Discovered, and Actions, with Status shown by default.
- The Status log must reflect the current state, including the already-installed notice and the conflict choice, and must not be repeated above the buttons.
- The Discovered log must list the facts the state was deduced from, with the command that produced them, and must gain the full report when Inspect runs.
- The Actions log must record every command that changed the system, with a timestamp and the output that says what was done.
- A conflict must be described from the owner's point of view: the mode says "another launcher already represents this application", not that the run is blocked.
- The main window must not show Type, Payload, or Embedded entry.
- The handler must be a single window: the action buttons sit below the details block, and a large text area below the buttons takes the remaining height and is initially empty.
- After Integrate succeeds, the window must follow the AppImage to its installed path, so File, Run now, and Inspect refer to the new location.
- The window must show the new location in its output after a move.
- Inspect must write its details into that text area.
- Integrate must write the integration result into that text area, and must list each existing launcher with its identifier, name, origin, version, target AppImage, icon, window class, and whether the target still exists.
- The window must remember its size on resize and on close, and honour a remembered size even when it is larger than the reported work area; only the position is clamped on-screen.
- The handler must set its program name to `appimage-activator`, and the entry must set `StartupWMClass=appimage-activator`, so the dock matches the running window to the entry and shows the activator icon.
- When conflicts exist, the main window must add only "This application is already installed." and "Integrate will show details, then offer to replace or add alongside."
- The window must be centred where the platform permits it, and must remember its position where the platform permits it; a restored geometry must be clamped so the window is entirely on the screen.
- Position cannot be restored under Wayland, which does not let a client choose its own placement; the handler must not fail when that is the case.
- The handler desktop entry must use the project's own AppImage Activator icon, installed into the user icon theme, and the AppImage MIME types must name that icon as their generic icon.
- The right-click "Open With" item must be named `AppImage Activator`, because `AppImage Handler` is another project's name.
- The entry, its record, and its icon must be `appimage-activator.desktop`, `appimage-activator.manifest`, and `appimage-activator.svg`.
- `handler install` must migrate the pre-rename `appimage-handler.desktop`, `appimage-handler.svg`, and `appimage-handler.manifest` away, and carry their recorded previous defaults into the new record.
- `handler status` must report a pre-rename entry that is still on disk and name the command that removes it.
- After Integrate is clicked and a choice is offered, the window must show a `Name:` label and a single-line editable field to the left of the buttons, prefilled with the `Name` from the AppImage.
- Add alongside and Replace existing must pass the field's value as `Name=` in the launcher, so several launchers for one application can carry their version or another hint.
- The window's application id must equal the launcher's file name: the program name is `appimage-activator` and no `Gtk.Application` id is set, because GTK would otherwise send the application id as the Wayland app id and GNOME would show a generic dock icon.
- `install --name NAME` must set `Name=` in the launcher while conflict detection keeps using the name from the embedded entry.
- Every install must state which operation it performs: a new integration, an update in place, a repair of a moved AppImage, a replacement, or an add alongside.
- The managed directory must be created when it is missing, and removing it must not make the tool mistake the AppImage for a different one: the identifier comes from the file's contents, so a moved AppImage is a repair.
- `list` must mark a record whose AppImage is gone, and `audit` must report its missing `Exec` and `TryExec` targets.
- `--replace` must work when the conflicting launcher sits at the target identifier, backing it up and restoring it on uninstall.
- Output must not be duplicated by forked children: flush buffered output before forking.

## Behavior

- After `handler install`, `xdg-mime query default application/vnd.appimage` reports `appimage-activator.desktop`.
- After `handler uninstall`, the previously recorded handler is restored.
- With no display, `handle` prints the run, install, and inspect commands instead of failing.

## Dependencies

- `09-appimage-integration.md`
