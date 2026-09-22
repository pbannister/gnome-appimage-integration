#!/bin/sh
#
# Portable test: the graphical handler is present, valid, and wired up.
# The GTK window itself cannot be exercised headlessly; this checks the script,
# the GTK bindings, and the headless fallback path.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

FILE_UI="$DIRECTORY_BUILD/appimage_handler_ui.py"
if [ ! -f "$FILE_UI" ]; then
    fail_test "the graphical handler was not copied next to the tool"
fi

if ! python3 -m py_compile "$FILE_UI"; then
    fail_test "the graphical handler does not compile"
fi

# With no display the handler must fall back to printed instructions, never hang.
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$DIRECTORY_BUILD/appimage-integrate" handle "$DIRECTORY_TEMP/Missing.AppImage" \
    > "$DIRECTORY_TEMP/handle.txt" 2>&1 || true
grep -q 'appimage-integrate' "$DIRECTORY_TEMP/handle.txt" \
    || fail_test "the headless handler printed no instructions"

# The handler must look for the UI script next to the tool.
grep -q 'appimage_handler_ui.py' "$DIRECTORY_BUILD/appimage-integrate" \
    || fail_test "the tool does not reference the graphical handler"

if ! python3 -c "import gi; gi.require_version('Gtk','4.0'); from gi.repository import Gtk" 2>/dev/null; then
    echo "SKIP: GTK4 Python bindings are not available (tool-gated)"
    pass_test "graphical handler (script only)"
    exit 0
fi

pass_test "graphical handler"
