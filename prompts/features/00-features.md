# Feature Index

- `01-site-build.md` — Site Build: generates `site.out/` from `site.in/`.
- `02-project-pages.md` — Project Pages: the standard published page set (status, dashboard, condensed todo/prompts/documents).
- `03-appimage-reader.md` — AppImage Reader: reads the AppImage container and its SquashFS payload.
- `04-desktop-entry-reader.md` — Desktop Entry Reader: parses `*.desktop` files.
- `05-desktop-entry-locator.md` — Desktop Entry Locator: finds `*.desktop` files through the GNOME and XDG search path.
- `06-command-line-tools.md` — Command-Line Tools: `appimage-inspect` and `desktop-inspect`.
- `07-icon-theme-locator.md` — Icon Theme Locator: resolves an icon name through the icon theme.
- `08-mime-association-reader.md` — MIME Association Reader: resolves which application opens a MIME type, and from where.
- `09-appimage-integration.md` — AppImage Integration: plan, install, uninstall, run, and audit.
- `10-appimage-handler.md` — AppImage Double-Click Handler: the opt-in `*.AppImage` handler.

- `01-site-build.md` and `02-project-pages.md` are shared with the skeleton and extended here; the project keeps them for the published page set.
- Features `03` through `10` implement this project's stated purpose.
- Every top-level requirement carries a stable `<FEATURE-NAME>-R<NNN>` identifier, and the tasks in `prompts/tasks/` claim those identifiers in `TASK-ACCEPTANCE`.
