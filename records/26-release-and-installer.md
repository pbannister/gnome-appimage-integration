# Record: a release, and a script that installs it

## Outcome

The project now publishes GitHub releases, and a user installs them with one command:

```sh
curl -fsSL https://raw.githubusercontent.com/pbannister/gnome-appimage-integration/master/scripts/install.sh | sh
```

Three scripts do the work, driven by `make release` (package the assets) and
`make release-publish` (tag and publish them).  `TODO.md` lost its completed items and now holds
only what is still open.

## The Pieces

| File | Role |
| ---- | ---- |
| `scripts/release-package.sh` | turns a built tree into the release assets: the tarball, `SHA256SUMS`, `RELEASE-NOTES.md`, and a `VERSION` file |
| `scripts/install.sh` | what a user fetches and pipes into a shell: download, verify, install |
| `scripts/release-publish.sh` | tags the commit and publishes the assets through the GitHub CLI |
| `tests/03-release-install.sh` | exercises the package and the installer against a local HTTP server |

## Asset Names Carry No Version

The tarball is `gnome-appimage-integration-linux-<arch>.tar.gz`, with no version in the name.  That
is deliberate: GitHub serves

```
https://github.com/<repository>/releases/latest/download/<asset>
```

without an API call, so the installer needs no JSON, no token and no rate limit.  A version can
still be pinned by putting the tag in the path, which is what `VERSION=<tag>` does.  The release
*title* and the tarball's `VERSION` file carry the version the binaries report, which the build
stamps from the commit.

Packing is deterministic — sorted names, no owner, the commit's own timestamp, and `gzip -n` — so
packing the same build twice produces one digest, and the digest in `SHA256SUMS` means something.
Building again does not reproduce it: the build counter that `--version` reports changes with every
build.  The test packs twice and compares, which is how the property is held in place.

## The Installer

It is POSIX `sh` on purpose, because it is piped into whatever shell the user has:

1. work out the machine (`linux-x86_64`, `linux-aarch64`), and refuse politely on anything else;
2. fetch the asset and `SHA256SUMS` from the release, through `curl` or `wget`;
3. compare the digest and refuse a mismatch — a digest that cannot be fetched is an error unless
   `APPIMAGE_INTEGRATION_SKIP_VERIFY=1` says otherwise, because "verified" must not be claimed
   when nothing was;
4. unpack into `$PREFIX` (default `$HOME/.local`), which is the layout the tarball already has;
5. where the asset does not exist for this machine, build from the source archive instead —
   `cmake`, a C++ compiler and zlib, with GTK4 optional — unless
   `APPIMAGE_INTEGRATION_NO_BUILD=1`;
6. report what was installed, warn when `$PREFIX/bin` is not on `PATH`, and print the two opt-in
   follow-ups: `handler install`, and `install <AppImage>`.

It installs the tools only.  Making this the `*.AppImage` handler rewrites the user's MIME defaults
and previous handler record, and the project's own best-practice checklist says that step must be
explicit — so the installer prints it rather than doing it.

## Testing Without GitHub

Both the packaging and the installer take overrides that make them testable hermetically:
`APPIMAGE_INTEGRATION_RELEASE_URL` replaces the GitHub base URL, and
`APPIMAGE_INTEGRATION_TARBALL` installs a local file.  `tests/03-release-install.sh` serves the
newly packaged release from `127.0.0.1`, installs it into a temporary prefix, runs the installed
tool, and then tests the two refusals: a tarball with one junk byte appended is refused and nothing
is installed, and a release with no asset at all says which asset it looked for.  A local-tarball
run covers the offline path.  No test uses the outside network.

## Publishing

`scripts/release-publish.sh` refuses a dirty tree, requires `gh` to be authenticated, builds from
the commit being released (and refuses a build whose version does not name that commit), packages
the release, tags `v<version>` (overridable with `TAG`), pushes the branch and the tag to
`PUSH_REMOTE` (default `origin`), and creates the release with the tarball and `SHA256SUMS` as
assets.  `TITLE` overrides the title.  Without `TAG`, the tag the commit already carries is
published; only a commit with no tag gets an invented `v<version>`.  An existing tag has to be on
the commit being published, or the run stops: a release that does not match its tag is worse than no release.  A push is skipped
when the remote already has the ref, which is safe because reading a public repository needs no
credentials — and it is what lets the release be published on a checkout whose push credentials
are not configured, once the commit and tag are there.

## The First Release

The tag is **v2026.09.23**, chosen over the exact build string for readability; the binaries still
report `2026-09-23-master-757d174`, so a download can be traced to a commit.

Publishing is left to the owner, because the GitHub API needs a token and this machine has none:
`gh` is not authenticated, while `git` pushes work over the SSH key that is present.  The commits
are pushed, the tag is pushed, the assets are packaged in `dataflow.out/release/`, and the one
remaining command is

```sh
TAG=v2026.09.23 make release-publish
```

after `gh auth login` — with the tag already on the commit, plain `make release-publish` runs it,
because the tag the commit carries is the one that gets published.  The script is written to be run
again safely: an existing tag is reused, the branch and tag pushes are skipped when the remote
already has them, and an existing release has its assets re-uploaded.

## Verification

Verified 2026-09-23: `make test` passed every script including the new
`tests/03-release-install.sh`; `make clean` followed by `make build` rebuilt the tree from nothing,
and the package built from that clean tree was installed and run by the installer.

## Commits

- `22f46ce` Publish a release, with a script that installs it
- `757d174` Let make clean remove the release directory too
- `1b1b627`, `2f1414b` Record: the release tooling
- `5b4cf58` Skip a push the remote already has, and keep a release on its tag
- `d18cdb1` Publish the tag a commit already carries instead of inventing one
- `6171dd2` Record: the handover for the first release
