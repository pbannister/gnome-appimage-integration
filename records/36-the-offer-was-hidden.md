# Record: the firejail offer was there and invisible

## Outcome

The offer existed, the tool reported firejail as installed, and the window never showed the
checkbox.  A person looking at the interface saw nothing about firejail, which is precisely what
was reported, and the test that covered the feature passed throughout.

Two faults, both found by looking at the running program rather than at the report:

| Fault | What it was |
| ----- | ----------- |
| The checkbox was built after the pass that decided whether to show it | `update_details()` ran before `p_sandbox_box_` existed, so `update_sandbox_offer()` found nothing to act on and returned; nothing asked again |
| The path the desktop actually takes was never exercised | `handle` prefers the graphical activator and falls back to a zenity list only when there is none, so the fallback in `tests/74` was driving an interface that a desktop with the activator never sees — and the fallback had no firejail option at all |

The first is why the user saw nothing.  The second is why the test was happy about it.

## Why the test did not catch it

It asserted the *data*, not the *widget*: `firejail_offered()` reads the tool's JSON, so the
report line said `firejail` whether or not anything was on screen.  The report line now states
visibility as well —

```
=== sandbox offer: firejail shown ===
=== sandbox offer: (none) hidden ===
```

— and the test asserts `shown`, which is the assertion that would have failed.  The general
lesson is the one this project keeps relearning in different clothes: a test that asks the
program what it knows is not a test that asks what a person sees.

## The fallback, and which interface is the real one

`appimage-integrate handle` is what the desktop runs for a double-click.  It prefers
`appimage-activator` when a display is present and one is installed beside the tool, and falls
back to a `zenity --list --radiolist` when it is not — so on a machine with the activator the
list is never drawn, and the activator is where the offer belongs.  Both now offer it:

- the activator has the checkbox, shown only when the tool reports firejail installed;
- the list has an `Integrate under firejail` row, added only when firejail is present, and the
  text fallback for a machine without zenity names the command.

Testing the fallback needed the activator to be genuinely unavailable, because `handler_ui_program`
looks for it beside the tool and one level up: the test copies the tool into a directory of its
own, which is what makes the fallback the path under test.

## Verification

- The installed build and the tree were driven the same way.  The installed one printed the offer
  with no visibility, meaning hidden; the tree printed `firejail shown`.
- `tests/95` asserts `shown` for the offered case and `(none) hidden` for a machine without
  firejail; `tests/74` drives the fallback list with a stand-in zenity and asserts both that the
  row is offered and that the choice reaches the launcher's `Exec`.
- `tests/74` also stopped assuming firejail is absent: the host has one now, so the absent case
  gives the tool an empty `PATH` instead of hoping.  That is the same trap as records 31, 33 and
  34, appearing for the fourth time, and it was caught the same way — by running the suite on a
  machine that had changed underneath it.
- `make test` passes.

## Note for a later session

Fixing the source did not fix what the reporter sees: `~/.local/bin` holds a build from before
the change, installed from a release tarball.  A fix in the tree is not a fix on the desktop until
the package is rebuilt and installed, and the version string is how to tell them apart.
