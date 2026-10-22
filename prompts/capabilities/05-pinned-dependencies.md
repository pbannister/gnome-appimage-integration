# Pinned Dependencies

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

- Pin a vendored or forked dependency to a release tag, never a branch: a
  branch pin cannot be rebased against a fixed base.
- Record the pin in the source README or the document that owns it.
- A test asserts the pin in both places: the script's constant and the
  recorded value match, and the pin has the expected shape.
- When the heavy dependency cannot run in the test environment, the test
  asserts the script interface and the pin instead, and cross-checks an
  implementation against the system tool where one exists.
