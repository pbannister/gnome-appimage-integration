# Record: the launcher context menu, and why "App Details" cannot be removed

## Outcome

An integrated launcher now offers **AppImage Activator** in its right-click menu, which opens the
graphical activator on that AppImage. The shell's own **App Details** item cannot be removed from
a desktop entry, and the reason is in the shell's source.

## App Details Belongs to the Shell

GNOME Shell 46 builds the application menu in `js/ui/appMenu.js`. Desktop actions from the entry
become menu items, and one more item is added by the shell itself:

```js
    _updateDetailsVisibility() {
        const sw = this._appSystem.lookup_app('org.gnome.Software.desktop');
        this._detailsItem.visible = sw !== null;
    }
```

Visibility depends on nothing but whether GNOME Software is installed: not on the application, its
desktop file, its AppStream metadata, or the MIME type it handles. Activating it calls
`org.gnome.Software` with the app id and the `details` action. The item is therefore not our
launcher's business and there is no key that suppresses it — the earlier question "are broken
desktop files reported" style of fix does not apply here, because nothing is broken.

The ways to remove it are outside the entry: remove GNOME Software, or hide the item with a GNOME
Shell extension. Neither is done here. The launcher offers its own item instead, which is the part
that was ours to fix.

## The Action Ours to Add

Each launcher now writes:

```
Actions=AppImage-Activator;Remove-AppImage;

[Desktop Action AppImage-Activator]
Name=AppImage Activator
Exec=<tool> handle <AppImage>

[Desktop Action Remove-AppImage]
Name=Remove this AppImage
Exec=<tool> uninstall --identifier <id>
```

The shell lists actions in the order of `Actions=`, so the activator comes first and Remove stays.
`handle` is the same entry point the double-click handler uses, so the action opens the same
window, on the same file, with the same three logs. The path is written through `exec_quote`, so
an AppImage whose name contains a space is quoted, and a test asserts that with
`Repair Copy.AppImage`.

Because this is written at integration time, launchers integrated earlier do not carry the new
action until they are integrated again; the entry is regenerated from the embedded one, so
re-integrating is safe and is the documented repair path.

## Tests

- `tests/integration_plan_test.cpp` asserts the action list, the group, its name, and that its
  `Exec` runs `handle` on the planned AppImage.
- `tests/90-integration-conflicts.sh` asserts the same against a real installed launcher, and that
  a path with a space is quoted in the action's `Exec`.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed. The generated launcher was
read directly, and the shell behaviour was read from the GNOME Shell 46 source rather than
inferred.

## Commits

- `3bb5a15` Open the activator from the launcher's context menu
