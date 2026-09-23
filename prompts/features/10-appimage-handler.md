# Feature: AppImage Double-Click Handler

## Purpose

A double-clicked AppImage must do something useful instead of nothing. The project provides a
handler that asks the user what to do, and manages its own registration as the `*.AppImage`
default.

## Requirements

- The project must provide a handler that accepts one AppImage path.
- The handler must read the embedded application name for its prompt.
- The handler must offer Run once, Integrate, Inspect, and Cancel.
- The handler must use `zenity` when it is available and a graphical display exists.
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
- The handler must prefer a GTK dialog when PyGObject and GTK4 are available, and fall back to zenity, then to printed instructions.
- The main window must show the application Name, Comment, and GenericName, the detected version, and a details block containing File, Size, and Will install as.
- The main window must not show Type, Payload, or Embedded entry.
- The handler must be a single window: the action buttons sit below the details block, and a large text area below the buttons takes the remaining height and is initially empty.
- After Integrate succeeds, the window must follow the AppImage to its installed path, so File, Run now, and Inspect refer to the new location.
- The window must show the new location in its output after a move.
- Inspect must write its details into that text area.
- Integrate must write the integration result into that text area, and must list each existing launcher with its identifier, name, origin, version, target AppImage, icon, window class, and whether the target still exists.
- The window must remember its size on resize and on close, and honour a remembered size even when it is larger than the reported work area; only the position is clamped on-screen.
- The handler must set its program name to `appimage-handler`, and the entry must set `StartupWMClass=appimage-handler`, so the dock matches the running window to the entry and shows the handler icon.
- When conflicts exist, the main window must add only "This application is already installed." and "Integrate will show details, then offer to replace or add alongside."
- The window must be centred where the platform permits it, and must remember its position where the platform permits it; a restored geometry must be clamped so the window is entirely on the screen.
- Position cannot be restored under Wayland, which does not let a client choose its own placement; the handler must not fail when that is the case.
- The handler desktop entry must use the project's own AppImage Handler icon, installed into the user icon theme, and the AppImage MIME types must name that icon as their generic icon.

## Behavior

- After `handler install`, `xdg-mime query default application/vnd.appimage` reports `appimage-handler.desktop`.
- After `handler uninstall`, the previously recorded handler is restored.
- With no display, `handle` prints the run, install, and inspect commands instead of failing.

## Dependencies

- `09-appimage-integration.md`
