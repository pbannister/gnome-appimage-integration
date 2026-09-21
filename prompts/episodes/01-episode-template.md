# Episode Template

- Copy this file into `prompts/episodes/` with the next free number.
- Replace every `<placeholder>` with concrete content.
- Remove the Filled Example section before dispatching the episode.
- Follow `prompts/how-to-write-episodes.md`.

## EPISODE-GOAL
* One sentence stating the single goal of the episode.

## EPISODE-PHASE
* Optional: the project phase this episode advances, e.g. "phase 2". A phase
is a milestone grouping one or more episodes; see prompts/how-to-write-episodes.md.

## EPISODE-ACCEPTANCE
- Is the first criterion satisfied?
- Is the second criterion satisfied?
- Is the episode reviewable in one sitting?

## EPISODE-RISKS
- Riskiest assumption, phrased as a question?
- Next riskiest assumption, phrased as a question?

## EPISODE-TASKS
- One bounded step.
- One bounded step.

## EPISODE-OUTPUT
* The response representation.

## EPISODE-FILES
- `<repository-relative path>` — new
- `<repository-relative path>` — existing

## EPISODE-BRANCH
llm/episode-{number}

# Filled Example

This example shows the worked Site Build feature as an episode.

It is illustrative; it is not dispatched as-is.

## EPISODE-GOAL
Implement the Site Build feature from `prompts/features/01-site-build.md`.

## EPISODE-ACCEPTANCE
- Does `scripts/site-build.sh` generate `site.out/` from `site.in/`?
- Does `make test` pass from the repository root?
- Is the diff reviewable in one sitting?

## EPISODE-RISKS
- Is `sh` available in the target environment?
- Do the tests avoid generated output in source directories?

## EPISODE-TASKS
- Create `scripts/site-build.sh`.
- Create `site.in/hello.txt`.
- Add tests in `tests/`.
- Run `make test`.

## EPISODE-OUTPUT
Report the created files and the result of `make test`.

## EPISODE-FILES
- `scripts/site-build.sh` — new
- `site.in/hello.txt` — new
- `tests/01-site-build.sh` — new

## EPISODE-BRANCH
llm/episode-01
