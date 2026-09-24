# Phases

Project work proceeds in numbered phases. A phase is a milestone that
groups one or more episodes (the reviewable work units; see
`prompts/how-to-write-episodes.md` §9 and the homelab project-pages
conventions `documents/09-project-pages-conventions.md` §6).

**Change the current phase only when committing the project** (owner rule
2026-08-26): the phase belongs to this project, not to the homelab
registry. The homelab reads it from the generated `site.out/phase.txt`
(emitted by `scripts/site-condense.sh` from the `Current:` line below) and
shows it next to the activity status (active/planned/deferred/complete),
which the human declares in the homelab registry.

Current: phase 4 — publishing — complete

- Phase 1 — document the AppImage and desktop entry formats and implement both readers and the locator — complete
- Phase 2 — document the integration best practice, make icon, MIME, and parameter provenance overt, and implement plan, install, uninstall, run, audit, and the double-click handler — complete
- Phase 3 — migrate the remaining AppImageLauncher-created launchers and add an update path — complete
- Phase 4 — make the project installable by others: the project pages, a GitHub release, and a one-command install script — complete

States: `not-started` | `started` | `complete`. Keep this file in sync
with the episodes that advance each phase and with `TODO.md`.
