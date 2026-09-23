#!/bin/sh
#
# Portable test: the graphical handler is present, valid, and wired up, and the
# handler registration installs its own icon and points the AppImage MIME type at it.
# The GTK window itself cannot be exercised headlessly.
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

if [ ! -f "$DIRECTORY_BUILD/icons/appimage-handler.svg" ]; then
    fail_test "the handler icon was not copied to the build tree"
fi

# A second launch must open its own window for its own AppImage.
grep -q 'NON_UNIQUE' "$FILE_UI" || fail_test "the handler is not multi-instance"
# An existing integration must be reported, not silently replaced.
grep -q 'is already installed:' "$FILE_UI" || fail_test "the handler does not report existing integrations"

# The handler must look for the UI script next to the tool.
grep -q 'appimage_handler_ui.py' "$DIRECTORY_BUILD/appimage-integrate" \
    || fail_test "the tool does not reference the graphical handler"

# With no display the handler must fall back to printed instructions, never hang.
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$DIRECTORY_BUILD/appimage-integrate" handle "$DIRECTORY_TEMP/Missing.AppImage" \
    > "$DIRECTORY_TEMP/handle.txt" 2>&1 || true
grep -q 'appimage-integrate' "$DIRECTORY_TEMP/handle.txt" \
    || fail_test "the headless handler printed no instructions"

# Registering the handler is exercised in an isolated XDG home.
DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA/applications" "$DIRECTORY_DATA/mime/packages" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"
cat > "$DIRECTORY_DATA/mime/packages/appimage.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/vnd.appimage">
    <comment>AppImage application bundle (Type 2)</comment>
    <generic-icon name="application-x-executable"/>
    <glob pattern="*.AppImage"/>
  </mime-type>
</mime-info>
XML

run_in_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

FILE_HANDLER_ENTRY="$DIRECTORY_DATA/applications/appimage-handler.desktop"
FILE_HANDLER_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-handler.svg"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$FILE_HANDLER_ENTRY" ]; then
    fail_test "handler install did not write the handler entry"
fi
grep -q '^Icon=appimage-handler$' "$FILE_HANDLER_ENTRY" \
    || fail_test "the handler entry does not use the AppImage Handler icon"
grep -q '^StartupWMClass=appimage-handler$' "$FILE_HANDLER_ENTRY" \
    || fail_test "the handler entry has no StartupWMClass, so the dock cannot match the window"
if [ ! -f "$FILE_HANDLER_ICON" ]; then
    fail_test "handler install did not install the handler icon"
fi
grep -q 'appimage-handler' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "the AppImage MIME type does not use the handler icon"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler uninstall > "$DIRECTORY_TEMP/uninstall.txt" 2>&1
if [ -f "$FILE_HANDLER_ENTRY" ]; then
    fail_test "handler uninstall left the handler entry"
fi
if [ -f "$FILE_HANDLER_ICON" ]; then
    fail_test "handler uninstall left the handler icon"
fi
grep -q 'application-x-executable' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "handler uninstall did not restore the original MIME icon"

if ! python3 -c "import gi; gi.require_version('Gtk','4.0'); from gi.repository import Gtk" 2>/dev/null; then
    echo "SKIP: GTK4 Python bindings are not available (tool-gated)"
    pass_test "graphical handler (script only)"
    exit 0
fi

pass_test "graphical handler"
