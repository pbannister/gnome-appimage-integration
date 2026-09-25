# Suggested Episode Plan

This document breaks the phases of `PHASES.md` into a series of suggested
episodes. It is a plan, not a queue: episodes are dispatched by copying
`01-episode-template.md` into the next free number and filling in
`EPISODE-TASKS` and `EPISODE-OUTPUT` at dispatch time (see
`prompts/how-to-write-episodes.md` §7).

Rules the plan follows:

- One episode, one goal, one acceptance, reviewable in one sitting.
- Every acceptance criterion is a question.
- Riskiest assumption first in `EPISODE-RISKS`.
- Episode numbering is assigned at dispatch (branch `llm/episode-<n>`). Here
  the episodes are named by phase and letter, so the plan survives
  renumbering.

Phase order is a dependency: a phase's episodes are dispatched only after the
previous phase's milestone is met. Within a phase, the human may reorder the
episodes.

## Phase 1 — <goal of the first milestone>

### Episode 1-A — <one goal>

## EPISODE-PHASE
phase 1

## EPISODE-GOAL
<The single goal, in one or two sentences.>

## EPISODE-ACCEPTANCE
- <Checkable criterion, phrased as a question>?
- <Checkable criterion, phrased as a question>?

## EPISODE-RISKS
- <The riskiest assumption, phrased as a question>?
- <The next assumption, phrased as a question>?

## EPISODE-TASKS
- <One bounded step>.
- <One bounded step>.

## EPISODE-FILES
- `<path>` — new
- `<path>` — existing

## Phase 2 — <goal of the next milestone>

### Episode 2-A — <one goal>

## EPISODE-PHASE
phase 2

## EPISODE-GOAL
<The single goal, in one or two sentences.>

## EPISODE-ACCEPTANCE
- <Checkable criterion, phrased as a question>?

## EPISODE-RISKS
- <The riskiest assumption, phrased as a question>?

## EPISODE-TASKS
- <One bounded step>.

## EPISODE-FILES
- `<path>` — new
