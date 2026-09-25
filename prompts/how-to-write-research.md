# How to Write Research

This document defines how a human directs a research or study series.
* Research precedes a design commitment; its findings feed the open questions
  in `TODO.md` before the specification is written.
* A study series compares several candidates on one set of criteria.
* Apply naming rules from `prompts/flavors/01-semantic-sort-naming.md`.

## 1. One Criteria File

- The criteria for a study series live in exactly one file, for example
  `prompts/common/04-<series>-criteria.md`.
- Revise the criteria file, not the individual study tasks, when the criteria
  change.
- Record each revision in a dated changelog at the end of the criteria file.
- The criteria file is authoritative: a study compares against it and does not
  modify it.

## 2. One Study per Candidate

- Give each candidate its own study note under `documents/`.
- Use exactly the headings the criteria file mandates, so parallel studies are
  comparable by construction.
- One candidate per study; do not merge candidates.

## 3. Evidence Rules

- Prefer primary sources; cite sources inline (URLs).
- Mark a claim `unverified` when no source supports it; do not guess.
- Distinguish what a project states from what has been observed in practice.
- Carry an `unverified` marking forward into the synthesis; never upgrade it.

## 4. The Synthesis

- After the studies, write a synthesis that pools them.
- The synthesis states what has been tried before, what worked, and what did
  not work.
- It produces an explicit **use** list and **avoid** list, each with reasoning
  and the source study.
- It recommends candidates to drop, with reasons; the human decides.
- It proposes revisions to the criteria that proved weak or missing, without
  modifying the criteria file.
- Every claim in the synthesis traces to a study note or the survey; no claim
  is added from outside those sources.

## 5. The Survey Is the Durable Index

- Keep one human-readable survey document with a `Status` section.
- Fold the synthesis findings back into that `Status` section, so a later
  session does not re-survey the same ground.
- A candidate discovered mid-series gets its own appended study; do not reopen
  the finished ones.

## 6. Where the Work Lives

- Research deliverables live in `documents/`.
- A task file in `prompts/tasks/` is a dispatch record; it may say "relocate to
  `documents/`" once the finding is durable.
- Dispatch studies concurrently when they are independent; review the
  synthesis before it becomes a design input.

## 7. Decision Records

When the research ends in a choice, write a decision record under
`documents/`:

- A dated header naming the decision and the documents it is an input to.
- The inputs and evidence it rests on.
- An options ladder: each rung a mechanism or option, cheapest first, with
  what it buys.
- The decision, with the rule that lower rungs are exhausted before paying the
  cost of the highest one (where that applies).
- The consequences, and what would reopen the decision.
- When a reader over-reads a requirement and the human corrects it, reword the
  requirement itself (the design document or feature file) and record the
  correction; do not only note the exception.
