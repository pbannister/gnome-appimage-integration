# How to Write Episodes

This document defines how a human must write episodes for the LLM.
* An episode is a bounded unit of strongly related work with one goal and one acceptance.
* An episode groups what would otherwise be several small items into a single work order.
* Apply naming rules from `prompts/flavors/01-semantic-sort-naming.md`.

## 1. Episode Purpose

- An episode minimizes dispatch overhead.
- Dispatch overhead is prompt setup, context loading, and human attention per handoff.
- An episode also minimizes coherence overhead.
- A model that handles the whole episode keeps the episode's context.
- A model that handles fragments loses context between fragments.

## 2. Episode Granularity

- Group strongly related items into one episode.
- Size an episode so one competent reviewer can review its complete diff in one sitting.
- Prefer one feature or one work product per episode; that is the observable proxy for reviewable size.
- A larger episode has vague acceptance and drifts.
- A smaller episode pays overhead per item.

## 3. Episode Structure

Every episode must contain these sections in order:

- EPISODE-GOAL — A clear statement of the single goal.
- EPISODE-ACCEPTANCE — Checkable criteria phrased as questions.
- EPISODE-RISKS (Optional) — Risky assumptions phrased as questions, in order of risk.
- EPISODE-TASKS (Optional) — The execution plan, one bounded step per bullet.
- EPISODE-OUTPUT (Optional) — The response representation.
- EPISODE-FILES (Optional) — The files involved in the episode.
- EPISODE-BRANCH (Optional) — The git branch name for the episode.

## 4. Dispatching an Episode

- Dispatch an episode file as the task for the LLM.
- The EPISODE-GOAL section is the requested work, equivalent to TASK-DESCRIPTION in `prompts/how-to-write-tasks.md`.
- The EPISODE-ACCEPTANCE and EPISODE-RISKS sections are constraints the model must verify.
- The EPISODE-TASKS section is the execution plan.
- An episode authorizes only the operations in its dispatched task definition. EPISODE-GOAL and EPISODE-ACCEPTANCE establish intent and acceptance; they do not authorize additional file operations.

## 5. Writing the EPISODE-ACCEPTANCE Section

- Phrase every acceptance criterion as a question.
- A question has a checkable answer.
- An episode is done when every question is answered.

## 6. Writing the EPISODE-RISKS Section

- Put the riskiest assumption first.
- Phrase each risk as a question.
- The model must verify risky assumptions before building on them.
- The model may draft risk questions.
- The human orders them and judges the answers.

## 7. Writing the EPISODE-TASKS Section

- Sub-tasks are the execution plan.
- Sub-tasks are drafted by the model and ratified by the human.
- Sub-tasks are not queue items.
- The queue holds episodes, not sub-tasks.

## 8. Relationship to Tasks and Features

- An episode contains task-level work.
- Task files in `prompts/tasks/` may serve as sub-task definitions.
- Feature files in `prompts/features/` define stable requirements.
- An episode must reference applicable feature files explicitly.
- An episode's goal does not widen the dispatched task's file scope (section 4).

## 9. Relationship to the Queue

- `TODO.md` holds episodes.
- A checked episode item records the outcome.
- Carry, defer, or drop episodes in the weekly grooming.

## 10. Intent and Record

- The episode file is intent.
- Intent is written before dispatch.
- The record is the outcome.
- After review, record the outcome in `records/` and reference the commit.

## 11. Episode Patterns to Avoid

- State the goal as a concrete outcome, not a vague wish.
- State at least one acceptance criterion.
- Put the riskiest item first in the plan.
- Split an episode whose review needs more than one sitting.
- Reference the existing task instead of duplicating it.

## 12. Human Override

- The override rules are in `prompts/01-contract.md` section 10.
