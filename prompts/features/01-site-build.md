# Feature: Site Build

## Purpose

The project contains a static-site input directory (`site.in/`) and a generated
static-site output directory (`site.out/`).

The Site Build feature provides the script that converts `site.in/` into
`site.out/`, so the published project pages are regenerated from their sources
rather than edited where they are served.

## Requirements

- `SITE-BUILD-R001` — `scripts/site-build.sh` must generate the static site in `site.out/` from the input in `site.in/`.
- `SITE-BUILD-R002` — Each `site.in/*.txt` input file must produce `site.out/<name>.html`.
- `SITE-BUILD-R003` — Every other file under `site.in/` (an authored asset such as a script, a style, or an image) must be copied verbatim into `site.out/`, so a page can reference it by relative path. `site.in/template.html` is structure, not a page, and `site.in/pages.nav` is input for the page set, not a published asset; neither is copied.
- `SITE-BUILD-R004` — A page may contain `__KEY__` placeholders for live state; values come from the state file (`dataflow.out/site-state.txt` by default, overridable with `SITE_STATE_FILE`), written by `scripts/site-state-fetch.sh`. When the state file is absent, remaining placeholders render as `unavailable`, so the build stays portable.
- `SITE-BUILD-R005` — Generated output must be identified as generated.
- `SITE-BUILD-R006` — The script must start from an empty output directory, keeping the tracked `.gitkeep` placeholder, so a renamed or removed page cannot linger in `site.out/` as a published orphan.
- `SITE-BUILD-R007` — The script must accept optional input and output directory arguments.
- `SITE-BUILD-R008` — When no arguments are given, the script must use `site.in/` and `site.out/` relative to the repository root.
- `SITE-BUILD-R009` — The script must be a POSIX shell script.
- `SITE-BUILD-R010` — The HTML page structure must live in `site.in/template.html`, not in the build script.
- `SITE-BUILD-R011` — `site.in/template.html` must contain the marker line `<!-- SITE-CONTENT -->` where page content is inserted.

## Behavior

- Running the script recreates `site.out/` from `site.in/`: one HTML page per
  `.txt` input, plus every authored asset.
- Re-running the script overwrites existing output deterministically, and a
  stale page or asset from an earlier build is removed rather than left behind.
- A page's `__TITLE__` placeholder becomes the page title; the index page uses
  the project title and every other page uses its title-cased basename.

## Dependencies

- None.
