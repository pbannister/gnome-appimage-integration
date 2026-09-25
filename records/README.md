# README for records

- The record of each episode lives here.
- A record is the outcome: what was done, what was decided, and where the work lives.
- A record is version-controlled.
- A record is written after the episode settles and the human reviews.

## Record Types

Three record forms have proven their worth in practice.

### Outcome record

- One file per episode.
- Name: `records/<number>-<episode-name>.md`, or
  `records/<YYYY-MM-DD>-<number>-<episode-name>.md` when a project writes many
  records per day and chronological sorting matters.

Example structure:

```markdown
# Record: <episode name>

## Outcome

<What was done, in a few sentences.>

## Decisions

- <A decision made during review>.

## Verification

<What tests or checks passed, with dates.>

## Commits

- `<commit hash>` <commit message>
```

### Incident record

- One file per incident.
- Written after an incident settles, before moving on.
- Captures root cause and lessons while the detail is fresh.

Example structure:

```markdown
# Record: <incident name> — <date>

## What happened

<What was attempted and what broke.>

## Root cause

<The underlying cause, not the symptom.>

## Fix (applied, verified)

<What restored the system, and how it was verified.>

## Lesson

- <One lesson per bullet, phrased so a future session can act on it>.

## Safeguards for any retry

- <Non-negotiable preconditions for trying again>.

## Current state / next steps

<What remains, in order.>

## Commits

- `<commit hash>` <commit message>
```

### Handoff record

- Written at a session boundary.
- Purpose: a fresh session resumes the work without the prior conversation's memory.
- Lists the verified current state, then the next tasks in order.

Example structure:

```markdown
# Record: <area> handoff — <date>

> Written so a fresh session can resume without this conversation's memory.

## Current state (verified <date>)

- <Fact per bullet, each verified and dated>.

## Next tasks (in order)

1. <One ordered increment>.
2. <One ordered increment>.

## Commits (this session)

- `<commit hash>` <commit message>
```

## Rules

- Write the outcome record only after the episode settles and the human reviews it.
- A record cannot cite its own commit hash: commit the work first, then write the record in a second commit and cite the work commit. Never write a placeholder hash.
- A behavioral repair to executable code names its regression test in the record (see `prompts/02-workflow.md` section 4).
- A record must not quote the patterns a sanitization gate refuses; describe them generically, or the record refuses itself.
- Record an unattributable state change in `TODO.md` to confirm; do not silently revert it.
- "Unexplained" is a result: record it rather than guessing.
- A status update the task requires (in `TODO.md` or in a referenced record) rides with the work commit; the outcome record is a separate, post-review commit (see `prompts/02-workflow.md` §7.1).
- Reference the commit hashes.
- Do not paste model transcripts into records.
- Do not record generated output or logs.
- Mark every verification with its date: `verified 2026-08-22`.
- When a later session finds facts stale, append a correction with the new fact and date; do not silently rewrite the old record.
- Live-state facts belong in generated documents or in records with verification dates, never in unmarked prose.
- Prefer generated documents over hand-written ones for anything that reflects live state; see `prompts/03-conventions.md`.
- After an incident, write the incident record with root cause and lessons before starting new work.
- A handoff record warns the next session that earlier facts may be stale; include a drift table when addresses or states changed.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `README.md` — this file.
