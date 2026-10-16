# README for tests

- `scripts/tests-run.sh` runs every `tests/*.sh` in lexical order, runs them
  all even when one fails, and reports a `PASS`/`FAIL` summary; it exits
  nonzero when any test failed.
- Tests are numbered in reserved bands, so lexical order is execution order and
  areas do not collide:
    - `00`–`09` — repository plumbing, the site build, and prompt-contract validation.
    - `10`–`19` — readers and parsers.
    - `20`–`29` — locators and resolution.
    - `30`–`49` — containers and formats.
    - `50`–`59` — live-state.
    - `60`–`79` — command-line and integration.
    - `80`–`99` — lifecycle and release.
- Shared helpers live in `tests/lib/`, are sourced by a test, and are never
  executed by the runner.
- Tool-gated tests skip with `skip_unless_tool <tool>` (from
  `tests/lib/test_helpers.sh`) and the standard `SKIP: missing tool: ...`
  message.
- Test tiers are defined in `prompts/02-workflow.md` §4.1: portable,
  tool-gated, and live-state.
- A test never touches the real system: sandbox `HOME` and XDG paths into a
  temporary directory, serve HTTP on `127.0.0.1`, and use no outside network.
- A test that changes live state does not belong in `make test`.
- `tests/06-live-state.sh` exercises the live-state scripts against a sandbox: `scripts/live-state-snapshot.sh` writes a restore point, `scripts/live-state-reset.sh` returns a desktop to a clean start, and `scripts/live-state-restore.sh` puts the integrations back.
- `tests/07-corpus-run.sh` exercises `scripts/corpus-run.sh` against synthetic AppImages covering the three root-desktop-entry shapes, and asserts that a second run over the same corpus produces an identical catalogue -- the property that says nothing is inherited between runs.
- The AppImage corpus itself is not part of `make test`: it is gigabytes and needs the network.  `scripts/corpus-list.tsv` names the applications (a `local:` source means a file already on the machine), `scripts/corpus-fetch.sh` fetches them, and `scripts/corpus-run.sh` runs the tools over them, giving each application its own throwaway `HOME` and XDG tree and writing a catalogue under `reports/`.  It also verifies the launcher each install wrote -- the file, its `Exec` target, a `TryExec` target, and that the icon resolves -- because an install that exits 0 is not the same as something the desktop can run.  `scripts/lib/corpus-paths.sh` decides where all of that goes: the samples and reports on bulk storage (`/datasets/appimage-corpus` when `/datasets` exists, else `~/appimage-corpus`), and the throwaway desktops on the fast local filesystem under `${TMPDIR:-/tmp}`, because a spinning disk should not be seeking through a sandbox per application.  `APPIMAGE_CORPUS_DIR` and `APPIMAGE_SANDBOX_DIR` override both.
- `tests/09-prompt-contract.sh` validates the prompt corpus; `tests/10-prompt-validator.sh` is its adversarial corpus, injecting malformed fixtures and asserting each is rejected.
- `tests/71-desktop-integration-optout.sh` covers the AppImage specification's conditions against integrating: the user-level sentinel file, a set `$DESKTOPINTEGRATION`, and a running process named `appimaged` (a compiled stand-in, because the tool reads `/proc`). Each one is reported as a warning and the install proceeds anyway -- a warning, not a gate, which is the property the test exists to hold. `tests/desktop_integration_optout_test.cpp` covers the detection itself, including the `/usr/share` and `/etc` paths no test can create.
- `tests/73-appimage-fuse.sh` covers how a runtime mounts its payload: the reader finds the FUSE library a runtime asks for by name (a string in its own bytes, not a `dt_needed` entry), the plan reports it, and the plan says when this machine cannot mount. `tests/appimage_mount_test.cpp` covers the verdict itself, every combination of missing device, missing library and missing mount helper, which is what a test cannot arrange on a machine that has them. The shell test asks the host the same question the tool asks and insists on the matching answer, so it holds on a machine with a usable `/dev/fuse` as well as one without.
- `tests/74-firejail-sandbox.sh` covers the sandbox offer: refused when asked for and absent, refused when the name is unknown, `firejail --appimage` in the launcher's `Exec` when present, recorded, kept by a rewrite, reported by `audit` when the backend goes, and turned off again with `--sandbox none`.
- `tests/72-appstream-metadata.sh` covers AppStream metadata from the payload: installed under `$XDG_DATA_HOME/metainfo` named after the component id, recorded in the manifest, removed on uninstall, shared between two integrations of one component and removed only by the last of them, restored rather than deleted when a file was already there, installed once when a payload ships both suffixes, and warned about when its `<launchable>` names a launcher nobody wrote.
- Keep spec-derived sample inputs in `dataflow.in/` and use them as fixtures.

## Canonical Files

The following filenames are canonical and must not be renamed or duplicated without an explicit task:

- `lib/test_helpers.sh`
