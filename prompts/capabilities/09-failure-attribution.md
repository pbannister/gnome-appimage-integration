# Failure Attribution

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

Through a single event stream, a defect in the harness, a dropped packet in the transport, a dead sandbox, and a refused credential all arrive as the same silence.
Attribution is the work of deciding which layer failed before deciding what to change.

## 1. Name the layer

- Attribute a failure to one layer before fixing it: brain, harness, hand, credential, transport, or provisioning.
- A retry without attribution is a guess; record which layer was suspected and on what evidence.
- Report the layer with the failure, so the next session does not repeat the diagnosis.
- `records/README.md` owns the incident record; this capability requires that the incident names the layer and the evidence.

## 2. Make the run attributable

- Every run carries a session identifier that appears in its events, its log, and every record and log it produces.
- A failure report names the session identifier, the tool call, and the input that produced it.
- An artifact that cannot be tied to a session identifier is treated as unexplained.

## 3. Do not launder a failure

- A tool-call failure is passed to the brain as text and is reported; it is never converted into a success.
- `prompts/02-workflow.md` section 6 owns verification reporting, and `records/README.md` owns recording an unexplained result rather than guessing at it; this capability adds the layer and the identifier.
- A failure the agent cannot attribute stops the work and is reported, rather than being retried until it appears to pass.
