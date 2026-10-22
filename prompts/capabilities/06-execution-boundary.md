# Execution Boundary

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

The brain is the model and its harness; the hands are the sandboxes and tools that perform actions.
The brain reaches a hand only through a tool call, `execute(name, input) -> string`.
The boundary is therefore a property of the hand, not of the prompt: a rule the model is asked to obey is not a boundary.
Instruction-layer injection resistance is `prompts/01-contract.md` section 8; this capability closes the structural half.

## 1. The sandbox is cattle

- A sandbox is provisioned from a recipe, `provision({resources})`, when a tool call needs it.
- A sandbox is disposable: the brain discards it and provisions another rather than nursing it back to health.
- A tool-call failure is returned to the brain as a string, and the brain decides whether to retry; a retry is a decision, not a default.
- No unique state lives only inside a sandbox; durable state belongs in the repository or in a record.
- A stuck sandbox is evidence of a defect to fix, not a thing to tend.

## 2. Credentials are never reachable from the sandbox

- Code that the model generated, or that arrived from outside the project, runs where credentials are not reachable.
- The project uses one of two patterns, and names which:
    - auth bundled with the resource: initialize the sandbox with the authenticated remote already wired in, so the model never handles the credential.
    - a credential proxy: the proxy holds the credential and makes the call, and neither the harness nor the sandbox is aware of it.
- A task that needs a credential declares where the credential lives and which pattern carries it.
- A credential never enters the working tree, a prompt file, a task file, a record, a log, or output.
- Narrow scope on a credential is not a substitute for unreachability; a token that a smarter model cannot misuse is an assumption, not a boundary.
- A credential does not cross a delegation boundary (`prompts/capabilities/08-delegation-authority.md`).

## 3. Declare the boundary

- A task declares the reach a hand needs: which paths it writes, which commands it runs, and which hosts it contacts.
- Network egress defaults to denied; a task that needs a host names it.
- The test sandbox is a separate concern: `prompts/02-workflow.md` section 4.2 rules that a test is hermetic, which is a property of the test, not of the execution boundary.
- Confinement belongs to the platform that runs the sandbox; when the platform does not provide one, the project states that rather than assuming it.
- A hand's third-party checkouts follow `prompts/03-conventions.md` section 6.1.
- A project records which boundary its platform provides, so a checkout moved to a host without one is a known change and not a silent loss.
