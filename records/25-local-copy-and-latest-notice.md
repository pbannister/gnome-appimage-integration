# Record: use a local copy, and say when there is nothing newer

## Outcome

Two behaviours were added to the Update path:

- if the offered file is already beside the installed one, the update verifies it and uses it
  instead of downloading it again;
- when the check finds nothing newer, Status declares **You already are using the latest
  version.** in bold and the Update button is disabled.

Both were verified live against FreeCAD 1.1.3 through the installed tool, and both are covered by
the test suite.

## Using What Is Already There

`apply_update` computed the destination — the offered file's own name in the directory of the file
being updated — and then always downloaded. It now checks that destination first:

| | |
| --- | --- |
| destination exists, and is not the file being replaced | verify it exactly as a download would be verified, make it executable, and use it in place; nothing is fetched |
| destination exists but does not verify | refuse, naming the file and saying to remove it to download the offered file instead — a working file is never replaced by a broken local one |
| destination is the file being replaced | the ordinary download path, because the local file *is* the old one |

The verification is the same function the download path uses, so the published digest and the
file's own `.sha256_sig` still decide.  The reporting says which happened: *used the copy already
at …* against *downloaded from …*, and `--dry-run` names the local file rather than a URL.

This is the case that matters in practice: a newer AppImage downloaded by hand into the managed
directory, or an earlier integration's file, sitting next to the one in use.  It saves the whole
transfer — 820 MB for FreeCAD.

## Nothing Newer

`on_update()` already asked the tool what the transport offers.  Now the answer decides the panel:

| Check result | Status | Update button |
| --- | --- | --- |
| `same` or `same-file` | bold **You already are using the latest version.** | disabled |
| `older` | bold **The release is older than the version you are using.** | disabled |
| an update is available | the download starts | insensitive while it runs |
| the check could not be made | the reason, as before | left enabled, because a network failure is not an answer |

The bold line is a `GtkTextTag` on the Status buffer, applied after the text is set the same way
the Discovered tab emphasises `This run` and `Error`.  Clearing it happens when an update starts or
succeeds, so a window that has just installed something does not keep claiming to be up to date on
the file it replaced.

The test hooks grew two lines for this: `=== status in bold ===` and `=== disabled ===`, so a
driven run reports both without changing the `=== buttons:` row every other test reads.

## Fixing the Installer on the Way

`make install` failed with `Text file busy`: an activator window was open, and `cp` writes the
destination in place.  `scripts/program-install.sh` now copies to a temporary name and renames it
over the destination, which is atomic and leaves a running copy with the file it started from.
This is the ordinary case for a tool that installs itself an activator.

## Live Verification

- `appimage-activator --activate update` on the installed `FreeCAD_1.1.3-…AppImage`, driven through
  `~/.local/bin/appimage-integrate`, printed
  `=== status in bold ===` / `You already are using the latest version.` / `=== disabled === Update`.
- The FreeCAD integration itself is now 1.1.3, integrated and `properly integrated`, with the
  launcher, `TryExec` and the Update item all naming
  `~/Applications/FreeCAD_1.1.3-Linux-x86_64-py311.AppImage`, and 1.1.1 preserved as
  `FreeCAD_1.1.1-…AppImage.previous`.  An update on the real desktop completed at 16:09 and did
  exactly what this feature set describes — the new file under its own name, the replaced file
  kept — which also means it was that update, not a hand edit, that changed the FreeCAD launcher
  from the 1.1.1 that the previous record found.  With the local-copy rule now installed, the same
  click would have used the file that was already there instead of downloading it.

## Verification

Verified 2026-09-23 with `make test`; all twenty test scripts passed.

`tests/99-update-apply.sh` gained two sections: an offered file that is already present is used,
with the local file becoming the one in use, the launcher retargeted, and the replaced file kept as
`.previous` — proved by pointing the transport at a URL that serves **no** AppImage, so a download
would have failed; and a local copy that is not an AppImage is refused with the file named and the
working file untouched.

`tests/95-activator-ui.sh` gained the `same` and `same-file` cases, asserting the exact bold line,
the disabled Update button, and that nothing was downloaded.

## Commits

- `59195cd` Use an offered file that is already there, and say when nothing is newer
