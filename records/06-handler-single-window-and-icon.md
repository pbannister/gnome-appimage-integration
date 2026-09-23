# Record: single-window handler and its icon

## Outcome

The handler is now a single window with one large output area, it has its own AppImage icon,
and the AppImage MIME types use that same icon.

## Is There an Icon for the AppImage Standard?

There is no icon defined by the AppImage file-format specification.
The specification defines magic bytes, a payload, and an AppDir; it says nothing about an icon for the file type.

Three related icons do exist:

- The AppImage project has a project logo, shipped with AppImageKit, AppImageLauncher, and the runtime; it identifies the project rather than the file type.
- The shared MIME database gives the AppImage types a generic icon; the definition this host had used `<generic-icon name="application-x-executable"/>`, which is why an AppImage looked like any other executable.
- Every AppImage carries its own application icon for the application it contains, which is unrelated to the file type.

Decision: the handler now ships an original mark of its own, `sources/tools/icons/appimage-handler.svg`.
It is a self-contained package with a run control, so it reads as "an AppImage you can run", and it avoids reusing the AppImage project's logo.

## The Icon Is Wired In Three Places

| Place | What is written |
| ----- | --------------- |
| The handler launcher | `Icon=appimage-handler` in `appimage-handler.desktop` |
| The user icon theme | `$XDG_DATA_HOME/icons/hicolor/scalable/apps/appimage-handler.svg`, mode 644 |
| The AppImage MIME types | every `<generic-icon name="application-x-executable"/>` in `$XDG_DATA_HOME/mime/packages/appimage.xml` becomes `appimage-handler`, so file managers show the AppImage icon |

The MIME rewrite is reversible: the original file is copied to the tool's `backup/` directory before the edit and restored by `handler uninstall`, which also removes the installed icon.
Re-registering our own handler no longer overwrites the real previous defaults it recorded; the manifest keeps the original `appimagelauncher.desktop` values.

## The Single Window

- Header: the application name, then version and GenericName, then Comment.
- Details: File, Size, and Will install as. Type, Payload, and Embedded entry were removed as requested.
- Buttons: Run once, Integrate, Inspect, Close, directly below the details block.
- Output: one large monospaced text area below the buttons, taking the remaining height, empty at first.
- Inspect writes the full AppImage report into that area.
- Integrate writes the conflict list there first, then switches the buttons to Back, Add alongside, and Replace existing, and writes the install result there.
- Run once now reports the start line in the text area instead of only a desktop notification.
- The old separate Inspect, conflict, and result windows are gone.

## Centring

The window is centred where the platform allows it.
`center_window` computes the centre of the monitor work area and applies it with `xdotool` on X11.
Wayland does not let a client choose its position, so there the call is a no-op and the compositor places the window; GNOME centres a new toplevel window by default.

## Verification

Verified 2026-09-22 with `make test`; all fifteen test scripts passed.

- `tests/95-handler-ui.sh` now also registers the handler in an isolated XDG home and asserts: the entry contains `Icon=appimage-handler`, the icon is installed into the theme, the MIME generic icon is rewritten, and `handler uninstall` removes the entry and icon and restores the original MIME icon.
- On the real desktop, verified after `make install` and `handler install`: the entry carries `Icon=appimage-handler`, the icon is present and mode 644, both AppImage MIME types name `appimage-handler`, and the manifest still records the original `appimagelauncher.desktop` defaults.
- The GTK layout and centring could not be exercised headlessly; they are verified by compilation, import, and dispatch.

## Commits

- `cda59a3` feat: single-window handler with an AppImage icon, tied to the AppImage MIME types
