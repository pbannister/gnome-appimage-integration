# Record: integrated icons were invisible to GNOME

## Outcome

None of the AppImages integrated by this tool showed an icon.
The icons were on disk and resolved by this project's own locator, but GTK never looked at them,
because a stale icon theme cache made it skip the directory scan entirely.
Every install now refreshes that cache.

## Root Cause

GTK loads an icon theme directory only through its cache when that cache looks current.
In `gtk/gtkiconcache.c`, `gtk_icon_cache_new_for_path()` decides:

```
if (st.st_mtime < path_st.st_mtime)
  goto done;              /* cache outdated, so fall back to scanning */
... use the cache ...
```

and the caller in `gtk/gtkicontheme.c` does:

```
dir_mtime->cache = gtk_icon_cache_new_for_path (dir);
if (dir_mtime->cache != NULL)
  continue;               /* cached: never open the directory */
gdir = g_dir_open (dir, 0, NULL);
```

The cache is keyed to the **theme root** (`~/.local/share/icons/hicolor`), not to the size directories.
On this host:

| Item | Time |
| ---- | ---- |
| `~/.local/share/icons/hicolor` directory | 2026-09-21 14:05:48.000 |
| `~/.local/share/icons/hicolor/icon-theme.cache` | 2026-09-21 14:05:48.715 |
| the icons themselves, written into `64x64/apps`, `scalable/apps`, ... | 2026-09-22 |

Adding files to a subdirectory changes that subdirectory's mtime, not the theme root's, so the cache
stayed newer than the theme root and GTK kept using it. The cache contained no `appimage_` name, and
GTK never rescanned, so every icon this tool installed was invisible to GNOME.

## Fix

- `appimage_integrator_c::install` and `uninstall` now run `gtk4-update-icon-cache -f -t <hicolor>` (falling back to `gtk-update-icon-cache`) after icons change.
- `handler install` and `handler uninstall` do the same after the handler icon changes.
- `scripts/program-install.sh` refreshes the cache too, so `make install` alone is enough.
- `-t` ignores the missing `index.theme` in the user theme directory, which this host does not have.

## Secondary Fix: Invalid Size Directory

The NETGEAR AppImage ships an icon under a size directory literally named `0x0`, which no theme lists,
so that icon could never resolve. Such a directory is now remapped to `256x256` on install.
The icon already on this host was moved to `256x256/apps` and its manifest entry was updated.

## Verification

- Regenerated the real cache: `gtk4-update-icon-cache -f -t ~/.local/share/icons/hicolor` reported "Cache file created successfully", and the cache now contains `appimage_11aaf8de_org.freecad.FreeCAD`, `appimage_2bf60d0c_openshot-qt`, `appimage_57434599_OrcaSlicer`, `appimage_8e7f02a3_nsdt-app`, `appimage_a2188496_balena-etcher-electron`, `appimage_d787e1d1_cura-icon`, and `appimage-handler`.
- Every launcher this tool wrote has its `Icon=` value present in the cache (checked against the cache string table).
- Cache mtime equals the theme directory mtime, so GTK now accepts it as current.
- `make test` passed all fifteen test scripts; `tests/70-integration-sandbox.sh` now asserts that install creates the theme cache, and `tests/90-integration-conflicts.sh` asserts that no icon is installed under `0x0`.

## Note

GNOME Shell keeps icon themes in memory and watches the theme directories, so the icons should appear
without a restart. If they do not, logging out and back in clears the shell's in-memory theme.

## Commits

- `7d1bd78` fix: refresh the icon theme cache so GNOME resolves installed icons
