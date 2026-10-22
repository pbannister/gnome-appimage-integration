# README for evals

Behavioral evaluation cases for a real coding agent live here.

An eval is a frozen case: a task handed to a real agent, plus observable
expected behavior, scored by a human.

## What an Eval Is Not

- An eval is not a unit test. The tests under `tests/` validate the prompt
  artifacts; none of them can show how a model behaves.
- An eval is not deterministic. A pass raises confidence; it does not prove
  correctness.
- An eval never gates a commit. It informs a change to `prompts/`.

## When to Run

Run the cases after any change to a file under `prompts/`, and before adopting
that change. Run a single case when its behavior is the one at risk.

## How to Run One Case

1. Extract the task text, and only the task text:

   ```
   scripts/rules-eval-task.sh evals/<case>.md
   ```

   The task is the fenced block. Do not read it out by hand: the fence opens
   with a `## TASK-DESCRIPTION` heading, so taking "the text up to the next
   `##` heading" yields the string ```` ```text ```` instead of a task.

   The tool also checks the case's `## Requires` list against the project. When
   a prerequisite is absent it refuses, because a case written against a file
   the project does not have cannot be run or scored: a missing prerequisite is
   not a failure, it is not a run.
2. Read the prose outside that fence yourself. It is a note to the operator, and
   in one case it names the test the task contradicts — the exact thing the
   agent must discover, not be told. `--note` prints it, labelled.
3. Hand the extracted text to the agent as the task, in a scratch checkout or
   worktree (`prompts/02-workflow.md` section 3.1). A fresh session: context is
   part of what the rules steer, so a session carrying the change's discussion is
   not a valid subject.
4. Let the agent work to completion. Do not coach, and do not answer questions;
   a question it asks is itself an observation.
5. Audit before scoring: if the agent opened `evals/` or this file, the run is
   void, because it read its own rubric. Keep `evals/` in place during the run —
   `rules-check` verifies every vendored file, so removing one breaks `make
   test` and changes what is being tested.
6. Score every criterion in `## Scoring` as pass or fail, with the observation
   that decided it.

## How to Score

- Each criterion is pass or fail; there is no partial credit.
- Record `passed N of M` for the case.
- An unobservable claim by the agent is a fail, not a pass.

## Where Results Go

- Record the result in a `records/` outcome, incident, or handoff record; see
  `records/README.md`. Do not invent a record type for evals.
- Name the rules version the run tested, because a result is about that version
  and nothing else.
- A failure is a prompt defect. Fix the defect under `prompts/` in
  `00-project-rules`, cut a version, re-vendor, and re-run — never patch the
  project's vendored copy, which would hide the defect from every other project.

## Case Format

Each case file carries these headings, in this order:

- `## Case` — the behavior under test and why it matters.
- `## Requires` — the files the project must carry for the case to apply, or
  `Nothing` when the absent file is the subject. A case that assumes the
  skeleton's inventory is not portable, and must say so here.
- `## Task given to the agent` — a fenced block to hand over, optionally followed
  by prose for the operator.
- `## Expected behavior` — observable behaviors, one per bullet.
- `## Failing signals` — the observable defects that mean failure.
- `## Scoring` — the pass/fail criteria.

## Limits

- A case tests one behavior; a pass says nothing about the others.
- A model change, a tool change, or a larger context can change a result.
- The set is small on purpose. Add a case only for a behavior that failed, or
  that a change put at risk.
