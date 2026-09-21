# TODO

## Open Questions

* [ ] play with Jupyter and decide where it fits in the work pattern.
* [ ] decide the fate of the DELTA correction protocol.
* [ ] work through the async-worktree example in `documents/01-async-worktree.md` on a multi-thread development project.
      — The homelab exercise was single-threaded (hardware), so it used checkpoint and handoff records instead;
      worktrees remain the pattern for parallel, file-isolated development (see `documents/00-pattern-of-interaction.md`).
* [ ] improve the human-oriented documents in `documents/`.
* [ ] read the tool-universe sources in `documents/02-tool-universe.md`.

## Recently Completed

* [x] repair skeleton inconsistencies: `logs/`, `.gitkeep` files, `make test` wiring.
* [x] add worked example: Site Build feature, task, script, input, and tests.
* [x] harden the workflow: Definition of Done, git commit step, verification-failure loop.
* [x] define the workflow for "tasks" versus "features".
    * [x] should long form tasks be in `prompts/tasks/` and follow the `prompts/how-to-write-tasks.md` guidance?
    * [x] should there be guidance for how to write features in `prompts/how-to-write-features.md`?

* [x] include a map of the prompts in `prompts/README.md` if missing
* [x] include top-level map of project in `README.md` if missing

- [x] Create project structure.
- [x] Write initial prompts.
* [x] study `prompts/flavors/01-semantic-sort-naming.md` and remove similar naming rules from other prompts.
* [x] capture the interaction-pattern conversation in `documents/00-pattern-of-interaction.md`.
* [x] add the async-worktree worked example in `documents/01-async-worktree.md`.
* [x] add the tool-universe survey in `documents/02-tool-universe.md`.
* [x] add `prompts/how-to-write-episodes.md` and `prompts/episodes/01-episode-template.md`.
* [x] establish the intent/record separation with `records/README.md`.
* [x] register `documents/` and `records/` in the contract, conventions, README map, and skeleton test.
* [x] extract the site-build HTML structure into `site.in/template.html`.
* [x] use UPPERCASE names for constant shell variables in scripts and tests.
* [x] apply semantic-sort naming to shell constant variables.
* [x] apply type-prefix naming to shell variable names.
* [x] incorporate the homelab lessons into the skeleton (2026-08-23):
    * [x] add outcome, incident, and handoff record forms to `records/README.md`.
    * [x] add risky-operations and privacy-boundary rules to `prompts/common/02-universal-rules.md`.
    * [x] add test tiers (portable, tool-gated, live-state) to `prompts/02-workflow.md` §4.1.
    * [x] add generated-documentation and live-state-verification conventions to `prompts/03-conventions.md`.
    * [x] add skeleton-startup guidance and privacy-boundary note to `README.md`.
    * [x] add the worktrees-versus-records section to `documents/00-pattern-of-interaction.md`.
    * [x] add `documents/04-lessons-from-homelab.md` and register it in `documents/README.md`.
    * [x] note in `prompts/features/00-features.md` that bundled features are examples.
* [x] incorporate the homelab project-pages publishing conventions (2026-08-25):
    * [x] standard page set: rename `hello.txt` to `index.txt` (status page), add
          `dashboard.txt`, standard 5-page nav in `site.in/template.html`.
    * [x] add `scripts/site-condense.sh` (todo/prompts/documents generator, from the
          MI25 fan-service) and wire `make site` (`site-build.sh` + `site-condense.sh`).
    * [x] retire `make deploy` — publishing goes through the homelab (homelab-publish).
    * [x] add `documents/06-project-pages.md` (conventions summary) and
          `prompts/features/02-project-pages.md` (feature).
* [x] capture the MI25 fan-service project process lessons (2026-08-24):
    * [x] encode the record-update rule: when a task changes a status, update the
          referenced record in the same commit (the MI25 record `09-project-site.md`
          said "not yet implemented" after the deployment was live).
    * [x] document that generated build trees are path-bound: clean them when the
          repository is reached through a different path (SSHFS vs native mount; a
          stale CMakeCache broke the MI25 build after the mount path changed).
    * [x] add timestamped test-run logging to `scripts/tests-run.sh` (the MI25
          project writes `logs/YYYY-MM-DD-HH-MM-SS-test-run.log`).
* [x] decide the site-build approach: literal text-to-HTML, Markdown-to-HTML, or a static-site generator such as 11ty.
      — **Decided 2026-08-23 by the homelab exercise**: the literal text-to-HTML site-build is kept as the minimal
      worked example; a real site used Eleventy (a static-site generator); model-driven documents use a custom
      generator with a provenance header. Encoded in `prompts/03-conventions.md` §6.1.
