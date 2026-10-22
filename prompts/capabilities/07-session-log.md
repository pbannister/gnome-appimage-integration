# Session Log

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

A session is the append-only log of everything that happened in a unit of work.
It lives outside the harness and outside the model's context window, so that both can be replaced without losing the record.
The interfaces are `getSession(id)`, `emitEvent(id, event)`, and `getEvents()`, which returns positional slices of the log.

## 1. The session outlives the harness

- The harness is cattle: nothing in it needs to survive a crash.
- A harness that fails is rebooted with `wake(sessionId)`, reads the log with `getSession(id)`, and resumes from the last event.
- The harness writes each step with `emitEvent(id, event)` so the record is durable rather than reconstructed.
- State that exists only in a conversation is not a session log.
- The project names the durable artifact a resumption reads first: `TODO.md`, the record forms (`records/README.md`), or the repository itself.

## 2. Context is recoverable, not merely discarded

- Compaction summarizes the context window, and trimming removes tokens from it.
- Both are irreversible decisions about what later turns will need, and both are usually wrong about something.
- The session log is not the model's context window: material removed from the window stays recoverable in the log.
- Context management is the harness's concern; the log guarantees only that events are durable and available for interrogation.
- A resumption may rewind a few events before a moment to see the lead-up, or reread context before an action, by taking a positional slice.

## 3. Session boundary

- A session boundary is recorded before the harness is replaced, not after.
- `records/README.md` owns the handoff record and its rules; this capability adds only that the record is written from the log and carries the session identifier.
- A resumption that cannot locate its session identifier stops and asks rather than reconstructing the session from conversation memory.
