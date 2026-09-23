# Record: dock identity, remembered size, and placement

## Outcome

The handler now identifies itself to the desktop, honours its remembered size, and its remaining
limitations around placement are documented with the available choices.

## The Dock Showed a Generic Icon

A running window is matched to a desktop entry by its application id.
The handler runs as `python3 appimage_handler_ui.py`, so the id the compositor saw was `python3`,
which matches no desktop file, and the entry carried no `StartupWMClass` fallback for X11.

Fixes:

- the script calls `GLib.set_prgname("appimage-handler")` and `GLib.set_application_name("AppImage Handler")` before creating the application, so the Wayland application id is `appimage-handler`;
- the generated handler entry now carries `StartupWMClass=appimage-handler`, which is what X11 and XWayland match on;
- the entry's id is `appimage-handler.desktop`, which is the name the id matches, and its `Icon=appimage-handler` points at the icon installed in the user icon theme.

Verified on the real desktop after `handler install`: the entry now has both `Icon=appimage-handler` and `StartupWMClass=appimage-handler`.
If the dock still shows a generic icon, the diagnostic is to open Looking Glass (`lg`) and read the window's `app-id`, then report it.

## The Window Did Not Preserve Its Size

The saved state was:

```
main:   1397 x 1866
inspect: 1119 x 1080
result:  1210 x 1080
```

The screen is 5120x2160, so the remembered `main` size fits and should have been restored.
Instead it was being clamped to 1080, which is exactly the hardcoded fallback in `work_area()`
(`1920x1080`). The monitor query was failing and the fallback was silently limiting every window.

Fixes:

- `work_area()` now returns the union of every monitor work area, or `None` when the platform does not report one.
- A remembered size is honoured, bounded only by the minimum and a generous 16384 maximum; the work area no longer shrinks it.
- Only the **position** is clamped on-screen, which is the sanity check that was asked for.
- The reported work area is written into `ui.json` under `screen`, so a clamp can never be a mystery again.
- The size is saved on `default-width`, `default-height`, `maximized`, on close, and by a two-second autosave, so it survives any close path.

## Placement Choices

Wayland does not let a client choose or read its own toplevel position, and on this host
`org.gnome.mutter center-new-windows` is `false`, so the handler opens wherever the compositor puts it.

| Choice | How | Trade-off |
| ------ | --- | --------- |
| Let the compositor place it | nothing | no control over position |
| Centre every new window | `gsettings set org.gnome.mutter center-new-windows true` | session-wide, not specific to the handler |
| Position it from the client | install `xdotool`, run under XWayland with `GDK_BACKEND=x11` | position and centring work, at the cost of XWayland |
| Use a placement extension | a GNOME Shell extension that places windows by app id | another component to maintain |
| Anchor it as an overlay | a layer-shell client | GNOME does not implement `wlr-layer-shell`, so it is unavailable here |

The handler already reads and writes position through `xdotool` on X11, so the XWayland choice needs no code change.

## Reduced Conflict Notice

When an application is already installed, the main window now adds only:

```
This application is already installed.
Integrate will show details, then offer to replace or add alongside.
```

The full details still appear in the text area when Integrate is pressed.

## Verification

Verified 2026-09-22 with `make test`; all fifteen test scripts passed.

- `tests/95-handler-ui.sh` now also asserts the generated handler entry carries `StartupWMClass=appimage-handler` alongside `Icon=appimage-handler`.
- The real desktop was updated with `make install` and `handler install`, and the entry was inspected afterwards.
- The dock icon, the centring, and the remembered size cannot be observed from this headless session; their logic is verified by compilation, import, and dispatch, and the reported screen size will be visible in `ui.json` on the next run.

## Commits

- `7e89d44` fix: give the handler a dock identity and honour its remembered size
