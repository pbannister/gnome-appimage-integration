# Record: the graphical activator in C++

## Outcome

The graphical activator is no longer Python. `sources/tools/appimage_activator_ui.cpp` is a C++17
GTK4 program of the same size and behaviour as the 679-line PyGObject script it replaces, the
project is one language again, and nothing on the run-time path needs `python3`. The zenity and
printed-instruction fallbacks are unchanged.

## Why This Needed a Decision

The Python dialog existed because GTK4 *headers* were not installed on this host, only the
run-time bindings that ship with Ubuntu. A C++ window could not be built without `libgtk-4-dev`,
which is a new build dependency for a project whose binaries otherwise link only zlib, lzma, and
zstd. That was the reason for the choice, and it was never written down. The owner asked for the
conversion, so the dependency was accepted: `libgtk-4-dev` 4.14.5 is installed, matching the
run-time 4.14.5.

gtkmm was the alternative. It was rejected: the packaged version is 4.10, older than the GTK
run-time, and it pulls glibmm, cairomm, pangomm, and sigc++ for a window that the C API expresses
directly.

## What Was Built

| File | Role |
| ---- | ---- |
| `sources/tools/appimage_activator_ui.cpp` | the whole window, in the C API, with the same structure as the script it replaces |
| `sources/json/json_reader.{h,cpp}` | a minimal JSON value: parse, read members, and write back with sorted keys |
| `tests/json_reader_test.cpp`, `tests/14-json-reader.sh` | the reader's unit test |
| `sources/CMakeLists.txt` | `json_value` library, `appimage-activator` target behind `pkg_check_modules(gtk4)` |

The activator is built only where the GTK4 development files are present, so `make build` still
succeeds without them; `handle` then falls back to zenity. `scripts/program-install.sh` installs
the program and deletes any `appimage_activator_ui.py` or `appimage_handler_ui.py` left in
`$HOME/.local/bin` by an earlier install, so nothing shadows it.

`explain --json` is still the interface between the window and the tool, so the tool needed no
second output format; the reader has to exist because C++ has no `json` module. It is a value,
not a library: it parses what this project writes (including `\u00xx` escapes and raw UTF-8) and
keeps unknown members of `ui.json`, so editing one window's geometry does not drop the entries
this version does not use.

## Behaviour Kept

The header, the details grid (`File`, `Size`, `Integrate will`, `Will install as`, `Note`), the
conflict notice, the action row with the prefilled `Name:` field to the left of the buttons, the
initial and post-install button sets, the conflict text for upgrade, repair, and replacement, the
`--name` pass-through, following the AppImage after a move, the text area, the 2-second autosave
and the per-window geometry in `$XDG_DATA_HOME/gnome-appimage-integration/ui.json`, the migration
from the first geometry format, `NON_UNIQUE`, and the program name `appimage-activator` matching
`appimage-activator.desktop` are all as the Python had them.

Two options exist only for tests: `--activate ACTION[,ACTION...]` performs the action a button
would, and `--set-name TEXT` types into the Name field. The name is typed after each action,
because the action is what reveals the field. The desktop entry never passes either.

## Differences

- **The work-area clamp now happens.** The Python called `Gdk.Monitor.get_workarea()`, which GTK
  4.14 does not provide: `gdk_monitor_get_workarea` is absent from the library and PyGObject
  reports `get_workarea: False`. The `AttributeError` was caught, so `work_area()` always
  returned `None`, and the code never clamped a restored position or recorded the `screen` it
  saw. `gdk_monitor_get_geometry` does exist, so the documented behaviour now runs. This is an
  improvement, not a faithful copy, and it is deliberate.
- Start-up no longer pays a Python import: the C++ program starts in milliseconds instead of
  about 60 ms, and the process tree has no interpreter.
- The window can no longer be exercised by importing a module, so the test drives the real
  program; see below.

## Tests

`tests/95-activator-ui.sh` now:

- skips cleanly when `appimage-activator` was not built, so a machine without GTK4 development
  files still passes;
- reads the program name, the application name, `G_APPLICATION_NON_UNIQUE`, the absence of a
  `Gtk.Application` id, the "already installed" text, and the tool's `handler_ui_program` lookup
  from the source;
- compares the program name with the generated launcher's file name, the dock identity rule;
- drives the real window under `xvfb-run` with a stub tool, twice: once with `--set-name` to show
  the typed value reaching `install --name`, and once without, to show the field is prefilled
  from the AppImage;
- starts its own Xvfb and reads the live window's `WM_CLASS` with `xprop`, asserting
  `"appimage-activator", "appimage-activator"`.

`GDK_BACKEND=x11` is forced for every driven run. The host runs a Wayland session, and without it
GTK would prefer the session's compositor over the virtual display.

`tests/14-json-reader.sh` runs the new unit test: documents, escapes, numbers, booleans, errors
(unterminated strings, trailing commas, bad escapes, trailing text), the writer's sorted output,
the round trip that keeps an unknown `ui.json` entry, and the old geometry format.

## Verification

Verified 2026-09-23 with `make test`; all sixteen test scripts passed. The converted program was
installed to `$HOME/.local/bin`, and `appimage-integrate handle <AppImage>` was run against a
virtual display: the window it maps is named `AppImage` with
`WM_CLASS = "appimage-activator", "appimage-activator"`, so the tool execs the program directly.
The old `appimage_activator_ui.py` is gone from `$HOME/.local/bin`.

## Commits

- `08800ea` Port the graphical activator from Python to C++ with GTK4
