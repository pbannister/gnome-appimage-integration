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

## Behavior

- After `handler install`, `xdg-mime query default application/vnd.appimage` reports `appimage-handler.desktop`.
- After `handler uninstall`, the previously recorded handler is restored.
- With no display, `handle` prints the run, install, and inspect commands instead of failing.

## Dependencies

- `09-appimage-integration.md`
