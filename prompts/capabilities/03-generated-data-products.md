# Generated Data Products

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

A project that generates data (not only documents) follows the same
single-source rule, with a machine-readable contract:

- A pipeline stage has one number shared by its feature, script, test, and work
  product: feature `03`, script `scripts/03-<stage>.sh`, work product
  `dataflow.out/03_<name>.json`.
- Each stage reuses an existing work product; it rewrites the file only when
  explicitly asked (`--refresh`), and fills gaps left by partial failure with
  `--retry-failed` where that is normal.
- A reuse check compares the work product's content and timestamp; presence
  alone is not enough.
- The make rule names the file it produces, so a stage whose output exists does
  not run. Never declare a placeholder variable or a rule for a file no program
  produces: an unreachable prerequisite leaves the target permanently stale and
  re-runs it on every invocation.
- A step that annotates an existing work product in place instead of producing
  its own file is a phony target, and says so.
- A default `all` target builds the pipeline and the site, and is a cheap no-op
  when everything is current.
- Raw inputs are cached separately from work products (`dataflow.out/raw/`),
  with provenance: `SHA256SUMS` and `FETCHED-AT.txt`, fetched only under
  `--refresh`.
- Every generated data product is described by a `manifest.json` beside it,
  carrying its parameters, its entries, and provenance: source dataset,
  generating script, timestamp, and git commit.
- A `check` mode validates existing output against the manifest without
  rebuilding; `--check` is an accepted alias.
- `make clean` removes generated output but preserves raw caches. When a
  rebuild is expensive or can only be repeated against a live system, `clean`
  prints the command instead of running it.
- Verification is independent of construction: probe the artifact (re-read the
  file, ray-cast, signed volume, digest) rather than calling the builder's own
  helpers as the oracle.
- An exchange-format export (STEP, OBJ, glTF) declares its units, and a test
  parses the file back.
