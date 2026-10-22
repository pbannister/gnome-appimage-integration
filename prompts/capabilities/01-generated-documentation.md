# Generated Documentation

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

A project whose documents reflect live or derived state should keep a single source of truth and generate the documents from it:

- The model lives in `sources/`, for example `sources/<area>-model.yaml`.
- A generator script in `scripts/` produces the human documents from the model.
- The generator provides a validation mode that checks the model and exits nonzero on errors.
- Every generated document carries a provenance header: `Generated from <model> by <script> — do not edit by hand.`
- The documents index (`documents/README.md`) marks generated documents as generated.
- The generator's validation mode runs under `make test` when the project defines it.
