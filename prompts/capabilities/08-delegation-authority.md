# Delegation Authority

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

Decoupling the brain from the hands allows many brains and many hands: several harnesses, each reaching several execution environments.
Isolation is not authority. A worktree separates writers; it does not decide what a delegated agent may do.

## 1. Authority comes from the dispatch

- A delegated agent receives authority only from the task it is dispatched with.
- A delegation may not widen its own scope, and may not widen the scope of an agent it dispatches.
- Authority does not flow back up: a delegate's file operations are not the parent's authorization.
- When a delegation conflicts with the parent's task scope, the narrower scope governs.

## 2. Depth and budget are bounded

- The default delegation depth is one: an agent dispatches a delegate only when the task says so, and that delegate dispatches no further.
- A delegation declares a budget: the number of steps, tool calls, or elapsed time it may spend.
- A delegation that reaches its budget stops and reports; it does not extend itself.
- Recursion is a decision the human makes, not a default the agent takes.

## 3. Hands are passed, authority is not

- A hand is an execution environment reached by a tool call; a brain may be given a hand that another brain provisioned.
- Passing a hand passes a capability, not an identity: the receiving brain acts under its own authority.
- A delegate inherits neither a credential nor a session (`prompts/capabilities/06-execution-boundary.md`, `prompts/capabilities/07-session-log.md`).
- Concurrent writers still isolate per `prompts/02-workflow.md` section 3.1; delegation does not relax it.
- The parent owns the delegated work in the record; a delegate's verification does not discharge the parent's.
