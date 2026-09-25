# Project Pages (publishing conventions)

This document summarizes the project-pages publishing conventions that a
project started from this skeleton adopts. The canonical, authoritative
description lives in the homelab project:
`documents/09-project-pages-conventions.md`. This document is the skeleton's
summary of it and points back to the canonical source.

## Why pages exist

A project's published pages are its **read interface**: status, live state,
work plan (TODO), rules (prompts), and knowledge (documents). A human or LLM
can understand the project — and decide how to work with it — without
escaping the project's sandbox.

## The standard page set

A project publishes a directory tree served at `/projects/<id>/`, with a
consistent relative nav. Every page is required:

| Page | Content |
|---|---|
| `index.html` | **Status**: what the project is, its current status, how it works. |
| `dashboard.html` | **Dashboard**: live state (point-in-time values from the owning host). |
| `todo.html` | **Condensed TODO**: open work items. |
| `prompts.html` | **Prompts index**: the entire `prompts/` tree, grouped by directory, linking to full-text pages. |
| `documents.html` | **Documents index**: every file in `documents/`, linking to full-text pages. |
| `records.html` | **Records index**: every file in `records/`, linking to full-text pages. |

Rules:

- The generated pages (`todo.html`, `prompts.html`, `documents.html`,
  `records.html`) are **generated at build time** from the project's own
  files — never hand-maintained. The generator is `scripts/site-condense.sh`,
  wired into `make site` alongside `scripts/site-build.sh`.
- **Every `.md` file under `prompts/` (the entire tree), `documents/`, and
  `records/` is published as its own full-text page** (e.g.
  `prompts-common-02-universal-rules.html`), linked from the index pages.
  Markdown is rendered with pandoc when available (minimal fallback
  otherwise).
- `todo.html` stays condensed: open items with their continuation text,
  completed items summarized as a count.
- **Relative links only**, so the pages work at any depth under
  `/projects/<id>/`.
- **A page below the project root climbs out with `../`** — one level per
  directory. Apply the prefix to the nav block only, never to a page's own
  relative links; a nav bug at depth is invisible at the root, so test both
  shapes.
- **Replace the skeleton's placeholder `index.txt`/`dashboard.txt`** before
  registering: a published placeholder describes a project that does not
  exist.
- **Generate the dashboard from live state** (the state file, the tool's own
  output, or the build manifest) rather than maintaining live values by hand.
- A served asset that a page loads (script, style, image, model) carries a
  content version in its URL, including transitive imports, so a long-cached
  asset is re-requested when its bytes change.
- **The homelab owns the one navigation bar.** The template marks where it
  belongs with `<!-- HOMELAB-NAV -->` and must not contain a `<nav>` of its
  own; the homelab injects the bar (graphic, breadcrumb, and the project's
  page links) at publish time, and removes any project-authored `<nav>`.
  Projects never hardcode the labs site URL.
- **The page set is declared** in `site.in/pages.nav` (optional), an ordered
  `href|Label` list that must contain the standard set; `make site` writes
  `site.out/pages.txt` for the homelab (the standard set when there is no
  file). Extra pages are listed there and appear in the one nav.
- Pages are self-contained (their own `<style>`); the reference template is
  `site.in/template.html`.
- The `<title>` of every page must be a real title, never the bare file name.
- Recommended: a footer line linking back to `../` and naming the project.
- Generated output (`site.out/`) is gitignored; only `site.in/` is authored.

## Registration and publishing (homelab model)

- The project **registers once** in the homelab project's
  `sources/projects.yaml` (fields: `id`, `title`, `summary`, `status`,
  `visibility`, `self_published: true`, `pages_source`).
- **Activity vs phase** (owner 2026-08-26): the registry `status`
  (active/planned/deferred/complete) is the human-declared **activity**,
  in the single common homelab. The project's **phase** belongs to this
  project: declare it in `PHASES.md` (`Current: phase N — state`), change
  it only when committing the project, and let `scripts/site-condense.sh`
  emit `site.out/phase.txt` so the homelab can show "active · phase N
  started" on the projects page.
- `pages_source` tells the homelab how to read the project's generated tree:
  a path (`site.out/`, local or SSHFS) or an ssh command that streams the
  tree.
- **The homelab project is the only publisher.** It fetches every registered
  `pages_source`, runs the leak gate, and publishes with its own deploy.
  Projects do **not** push to the web server themselves — a project's
  `make deploy` is retired and only reminds you of this.
- Live-state freshness: the project refreshes its own state (`make state`,
  run on the owning host for hardware/network values) before the homelab
  publishes.
- **Live state is placeholders plus a fetched state file.** A page carries
  `__UPPERCASE_KEY__` placeholders; `scripts/site-state-fetch.sh` writes
  `dataflow.out/site-state.txt` as `KEY=value` lines (overridable with
  `SITE_STATE_FILE`), and `scripts/site-build.sh` substitutes the values.
  A placeholder with no value renders as `unavailable`, so the pages build
  on a host that never ran the state fetch. Sanitize values at capture: the
  publishing gate refuses absolute home paths, and the values land in HTML.

## Sanitization (non-negotiable)

Published pages must contain no MAC addresses, private LAN IPs, usernames,
home paths, or private-key material. The exact gate patterns live in the
homelab (`scripts/labs-deploy.sh`, `tests/03-labs-site.sh`,
`tests/06-project-pages.sh`); the homelab re-runs the gate at publish time.

The project runs its own gate first: `scripts/leak-gate.sh` over `site.out/`,
reading the patterns from `scripts/sensitive-patterns.sh` — their single
source. Do not inline the patterns in another script or test; a duplicated
pattern has already drifted once. A record or document that describes the
gate must not quote the refused patterns verbatim (that version refuses
itself). Fix a false positive by tightening the pattern, not by suppressing
the check: match private addresses only as dotted quads, so a numbered
heading such as `## 10. Human Override` still passes.

## Conformance checklist

1. Read the canonical conventions doc in the homelab.
2. Generate the standard page set per the rules above (the skeleton's
   `site.in/` + `scripts/site-build.sh` + `scripts/site-condense.sh` +
   `make site` produce it).
3. Register once in the homelab `sources/projects.yaml` with a readable
   `pages_source`.
4. Confirm the leak gate passes over the generated output.
5. After a homelab deploy, verify
   `curl https://labs.bannister.us/projects/<id>/` returns 200 and the
   content is sanitized.

## Live example

- `amd-mi25-fan-service` (SSHFS at `~/remote/beast.lan/work/...`) and
  `model-elevation-earth` (`~/work/01-model-elevation-earth/`)
  are live at `labs.bannister.us/projects/<id>/` using exactly this pattern.

## How this project does it

- `site.in/index.txt` — the status page, written for this project rather than
  left as the skeleton placeholder.
- `site.in/dashboard.txt` — the live state, as `__KEY__` placeholders.
- `site.in/pages.nav` — the page set, with **AppImages** as an extra page.
- `scripts/site-state-fetch.sh` (`make state`) — writes
  `dataflow.out/site-state.txt`: `FETCHED`, `HOST`, `VERSION`, `RELEASE`,
  `INTEGRATED_COUNT`, `MANAGED_DIR`, `HANDLER`, `AUDIT_ERRORS`,
  `AUDIT_WARNINGS`.  Values are sanitized of home paths (the publishing gate
  refuses them) and HTML-escaped, because they land in HTML.
- `scripts/site-build.sh` — substitutes `__KEY__` from that file; a key with
  no value becomes `unavailable`, so the pages build on a host where the tool
  is not installed.
- `scripts/site-condense.sh` — additionally generates `appimages.html`, the
  live inventory of what this tool has integrated on the owning host, read
  from the tool at build time so it cannot drift from the desktop.  It also
  reads the phase from `PHASES.md` (the state is the last field of the
  `Current:` line, so the line can carry a description as well).
- `scripts/leak-gate.sh` — the project-side sanitization check, reading its
  patterns from `scripts/sensitive-patterns.sh` (their single source) and
  running over `site.out/`.  `make site` runs it after the build, and `make
  check` runs it alone; the homelab re-runs its own gate at publish time.
