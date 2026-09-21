# Lessons from the Homelab Exercise

This document records what the first real application of this skeleton taught us.

The homelab project (74 commits over three days) applied the skeleton to a live network: inventory, two incidents, recovery, handoff sessions, live-state verification tests, and code-generated documentation.

Each lesson names where the skeleton now encodes it.

## 1. Bundled features are examples, not requirements

- The homelab built its public site with Eleventy, not the skeleton's site-build.
- The literal text-to-HTML site-build was too small for a real site.
- A real project must consciously keep, trim, or repurpose the bundled feature.
- Encoded in: `README.md` ("Starting a New Project from this Skeleton"), `prompts/features/00-features.md`.

## 2. Records were the workhorse; episodes were not used

- The homelab produced outcome, incident, and handoff records, and no episode files.
- Incident records captured root cause and lessons while fresh.
- Handoff records let a fresh session resume without the prior conversation's memory.
- Encoded in: `records/README.md` (three record forms).

## 3. Live-state verification tests are their own category

- Tests that SSH into real devices verified the documented model against live reality.
- PASS/WARN/FAIL classification with nonzero exit only on failure kept results honest.
- Encoded in: `prompts/02-workflow.md` §4.1 (test tiers).

## 4. Test environment dependence must be declared

- Tool-gated tests skipped cleanly; live-state tests required the real network and keys.
- A runner that stops at the first failure cannot mix tiers without a declared mechanism.
- Encoded in: `prompts/02-workflow.md` §4.1.

## 5. Single source of truth plus generated documents

- A network model in `sources/` and a generator script produced the diagram documents.
- Generated documents carried provenance headers and were never hand-edited.
- Encoded in: `prompts/03-conventions.md` §6.1.

## 6. Live-state facts go stale fast

- Addresses drifted within hours (Mikrotik .182 to .8, beast .146 to .20).
- Hand-off records appended drift warnings and correction tables.
- Encoded in: `records/README.md`, `prompts/03-conventions.md` §6.

## 7. Risky operations need change discipline

- Two incidents (an L2 loop, a broadcast storm) shared the same pattern: changes to a live network without staged verification.
- The lessons: stage a fallback, verify device behavior empirically, apply in small verified increments, agree an emergency brake, record the incident with lessons.
- Encoded in: `prompts/common/02-universal-rules.md` (Risky-Operations Rules).

## 8. Owner privacy boundaries need a first-class mechanism

- The homelab declared old work files off-limits; the directive held.
- Encoded in: `prompts/common/02-universal-rules.md` (Privacy-Boundary Rules), `README.md`.

## 9. Worktrees and records serve different concurrency

- The homelab was single-threaded by nature: concurrent changes against physical hardware are usually bad.
- Checkpoint and handoff records fit that shape; git worktrees still fit parallel file-isolated development.
- Encoded in: `documents/00-pattern-of-interaction.md` ("Worktrees versus Records").

## What held up unchanged

- The core prompts (contract, workflow, conventions, common rules) needed no changes during the exercise.
- The Makefile, test runner, and site-build script were reused as shipped.
- The skeleton's structure survived contact with a real, messy, hardware-touching project.
