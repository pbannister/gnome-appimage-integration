# Release and Publishing

Applies only when the project uses this capability; load it only for a task that touches it (see the Applicability section of `prompts/03-conventions.md`).

A release publishes an artifact; it must be the artifact of the commit it
names. The build-time version in `prompts/capabilities/02-version-information.md`
is what makes that checkable.

- A release refuses a version that carries `-changes` (the tree was dirty at
  build time) and a version that does not name the current short commit hash.
  `scripts/release-gate.sh` performs both checks.
- Build from the commit being released inside the publish path, so a stale
  build tree cannot be published.
- Pack deterministically: sorted names, no owner, the commit's timestamp, and
  no gzip timestamp, so packing the same build twice yields one digest.
- The canonical packaging script is `scripts/release-package.sh`; the canonical
  installer is `scripts/install.sh`. Publishing is project-specific (the
  reference project uses `scripts/release-publish.sh`).
- Name release assets without a version, so
  `releases/latest/download/<asset>` works without an API call; a version can
  still be pinned by adding the tag to the path.
- Publish a checksum (`SHA256SUMS`), a `VERSION` file, and generated release
  notes that carry the install and verify commands.
- An installer is POSIX shell (it may be piped), verifies the download against
  the checksum, and refuses when verification is impossible; a named override
  is the only way to skip the check.
- Installing the tools is safe; claiming a system default (a MIME handler, a
  desktop entry) is a deliberate step the installer prints, not performs.
- Installing over a running program writes a temporary name and renames it
  over the destination.
- A publish is re-runnable: reuse the tag the commit already carries, refuse a
  tag on a different commit, skip a redundant push, and re-upload assets.
- Generate a release tag as `v<date>-<branch>` when the commit carries no tag;
  `date` is the release date in `YYYY-MM-DD` form and `branch` is the current
  branch name. An explicit tag overrides the generated one; a checkout not on a
  branch requires one.
- A release title leads with the most significant information, so a narrow
  release list does not truncate it. For a dated release, start with the date in
  `YYYY-MM-DD` form, then the project name, then the branch and commit:
  `<date> — <project name> (<branch>-<commit>)`. An explicit title overrides the
  default.
