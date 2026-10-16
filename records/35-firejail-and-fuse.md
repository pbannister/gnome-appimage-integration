# Record: offering firejail, and asking whether an AppImage can start

## Outcome

Two questions about running an AppImage, as opposed to integrating it.

**firejail is now offered where it exists.** `install --sandbox firejail` and a *Run in firejail*
checkbox in the window, shown only where firejail is installed, write
`Exec=firejail --appimage <file> <field code>` — firejail's own documented form for an AppImage,
whose `--appimage` mode also keeps `private-bin` and `private-lib` out of the way, which is what
an AppImage needs. The choice is written into the record, so `refresh` and `update` keep it.
Asking for a sandbox that is not installed is refused rather than quietly dropped, an unknown
name is refused by the option, and a record whose firejail has since gone is reported by `audit`
and left alone by `refresh`.

**Whether an AppImage can start here is now detected.** A type-2 AppImage mounts its payload with
FUSE. The runtimes built before the static ones load `libfuse.so.2` by name — 15 of the 29 corpus
images do, 14 do not — and this dependency is invisible in the usual place: `readelf -d` on those
files lists `libdl`, `libpthread`, `libz` and `libc`, and no FUSE library, because the runtime
calls `dlopen`. The string is in the runtime's own bytes, beside the error it prints when the call
fails, and that is what the reader looks for. The plan reports the requirement, and warns when
this machine cannot satisfy it, naming the missing piece and `APPIMAGE_EXTRACT_AND_RUN=1`.

## Why the second question mattered more than expected

The host this was written on has **no `/dev/fuse` at all**, so no FUSE-mounting AppImage can start
on it — the whole corpus included — while every one of them integrates and verifies perfectly.
Integration and running are genuinely separate, and the tool had been reporting only the first.

It also widened the question beyond libfuse2. A runtime that links FUSE statically carries no such
string and runs where libfuse2 was never installed, and it still mounts: it needs `/dev/fuse` and a
`fusermount` just the same. So the device is the first question and the library is the second, and
the three answers — device, library, mount helper — are what the verdict is built from.

The detection also closed a real gap in `run`, which already fell back to
`APPIMAGE_EXTRACT_AND_RUN=1` when `/dev/fuse` was unusable but not when a runtime's library was
missing: on a machine with a working device and no libfuse2, it would have exec'd the file and let
it fail with `dlopen(): error loading libfuse.so.2`. It now asks the same question the plan asks,
and says which piece was absent.

## The Apparatus

| File | Role |
| ---- | ---- |
| `sources/appimage/appimage_reader.{h,cpp}` | reads the FUSE library name out of the runtime's bytes, bounded by a scan limit |
| `sources/integration/appimage_integrator.{h,cpp}` | `integration_sandbox_e`, the `Exec` shape, the record field, and `appimage_mount_problem()` with its thin host probes |
| `sources/tools/appimage_integrate.cpp` | `--sandbox`, the sandbox and mount fields in the JSON, `run`'s fallback, and the audit finding |
| `sources/tools/appimage_activator_ui.cpp` | the offer, the checkbox, and `--set-sandbox` for a driven run |
| `tests/appimage_mount_test.cpp` | the verdict, every combination, without needing a device that a test cannot conjure |
| `tests/73-appimage-fuse.sh` | a runtime with the string injected by `objcopy --add-section`, the reporting, and the host-aware assertion |
| `tests/74-firejail-sandbox.sh` | a firejail stand-in on `PATH`, the refusal, the `Exec`, the record, and the rewrite |

## Verification

- `make test` passes, including both new tests and the extended window test.
- The corpus was re-run: 29 applications, 28 `ok` and 1 installed alongside, 29 installs
  verified, 14 carrying AppStream metadata, **15 loading `libfuse.so.2` and 14 static**, and no
  application reporting an opt-out condition. The new `fuse` column was cross-checked against a
  fresh `appimage-inspect` over all 29 files, with no disagreements.
- That cross-check was worth running: an earlier ad-hoc count of the same corpus said 14, and the
  column said 15. The column was right and the ad-hoc shell was wrong, which is why the number in
  the documents is the one the catalogue and a fresh inspection agree on.
- The verdict function is unit-tested for every combination of missing device, missing library
  and missing mount helper, because a machine that has all three cannot be asked to pretend
  otherwise; `tests/73` then asks the host the same question the tool asks and insists on the
  matching answer, so it holds on either kind of machine.

## Note for a later session

`documents/13-sandboxing-appimages.md` records why firejail is the backend that was built and why
the container-shaped alternatives are not: `systemd-nspawn` and the container runtimes are the
wrong shape for a launcher, `bwrap` is the only candidate that could be exercised on this host,
and `systemd-run --user` cannot be verified here at all because this environment has no user
systemd bus. The `Exec` question that decides whether a backend is baked into the launcher is
settled for firejail by keeping `Exec` direct and refusing to rewrite a launcher whose backend has
gone; a second backend should follow the same rule.
