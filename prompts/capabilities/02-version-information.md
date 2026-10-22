# Version Information

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

A program version is a build-time fact, not a source literal.

- Derive the version from git state: `date-branch-hash` or `date-branch-tag-hash`.
- `date` is the build date in `YYYY-MM-DD` form.
- `branch` is the current git branch.
- `tag` is appended only when all sources are committed and the current commit is tagged.
- `hash` is the short git commit hash; it disambiguates builds from different commits, directories, and developers.
- Suffix the hash with `-changes` when the working tree has uncommitted changes.
- A build-time script generates a header that contains the version components and a build counter.
- The build counter increments on each build; it is not checked in.
- Keep the version-reporting function in a separate compilation unit, so a version change recompiles one unit instead of the whole program.
- The generated header carries a provenance header and is not edited by hand.
- The generated header is written to the build output directory, not into `sources/`.
- Example: `2026-08-24-master-a1b2c3d` or `2026-08-24-master-v1.0.0-a1b2c3d` or `2026-08-24-master-a1b2c3d-changes`.
- The canonical generator name is `scripts/version-generate.sh`.
- The canonical version header is `version_info.h`.
- The canonical version functions are `version_string()` and `version_build_counter()`.

Decided 2026-08-24: the git hash disambiguates builds, so the build counter
policy is settled. Keep the local counter gitignored; never check it in.
