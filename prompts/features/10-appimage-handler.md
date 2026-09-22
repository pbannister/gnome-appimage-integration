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
- Integrate must install the AppImage and then run it.
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
- The main window must show the application Name, Comment, and GenericName, the detected version, the file size, and the payload type.
- The main window must remember its size between runs, in the tool's state directory.
- Inspect must open a separate window whose only button is Close, and closing it must return to the main window.
- The Integrate dialog must list each existing launcher with its identifier, name, origin, version, target AppImage, icon, window class, and whether the target still exists.

## Behavior

- After `handler install`, `xdg-mime query default application/vnd.appimage` reports `appimage-handler.desktop`.
- After `handler uninstall`, the previously recorded handler is restored.
- With no display, `handle` prints the run, install, and inspect commands instead of failing.

## Dependencies

- `09-appimage-integration.md`
