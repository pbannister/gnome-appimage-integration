#!/bin/sh
#
# Portable test: the graphical activator is present, valid, and wired up, and the
# handler registration installs its own icon, names itself AppImage Activator for
# the right-click menu, and migrates the pre-rename files away.
# The GTK window itself cannot be exercised headlessly.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

FILE_UI="$DIRECTORY_BUILD/appimage_activator_ui.py"
if [ ! -f "$FILE_UI" ]; then
    fail_test "the graphical activator was not copied next to the tool"
fi

if ! python3 -m py_compile "$FILE_UI"; then
    fail_test "the graphical activator does not compile"
fi

if [ ! -f "$DIRECTORY_BUILD/icons/appimage-activator.svg" ]; then
    fail_test "the activator icon was not copied to the build tree"
fi

# A second launch must open its own window for its own AppImage.
grep -q 'NON_UNIQUE' "$FILE_UI" || fail_test "the activator is not multi-instance"
# An existing integration must be reported, not silently replaced.
grep -q 'is already installed:' "$FILE_UI" || fail_test "the activator does not report existing integrations"
# The window must present itself under the activator's name.
grep -q 'AppImage Activator' "$FILE_UI" || fail_test "the activator window is not named AppImage Activator"

# The activator must look for the UI script next to the tool. The compiler merges
# adjacent literals in the binary, so read the source rather than the executable.
grep -q 'appimage_activator_ui.py' "$REPOSITORY_ROOT/sources/tools/appimage_integrate.cpp" \
    || fail_test "the tool does not reference the graphical activator"

# With no display the handler must fall back to printed instructions, never hang.
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$DIRECTORY_BUILD/appimage-integrate" handle "$DIRECTORY_TEMP/Missing.AppImage" \
    > "$DIRECTORY_TEMP/handle.txt" 2>&1 || true
grep -q 'appimage-integrate' "$DIRECTORY_TEMP/handle.txt" \
    || fail_test "the headless handler printed no instructions"

# Registering the handler is exercised in an isolated XDG home.
if ! command -v xdg-mime >/dev/null 2>&1; then
    echo "SKIP: xdg-mime is not available (tool-gated)"
    pass_test "graphical activator (script only)"
    exit 0
fi

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA/applications" "$DIRECTORY_DATA/mime/packages" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc" "$DIRECTORY_XDG/home/.config"
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

FILE_ACTIVATOR_ENTRY="$DIRECTORY_DATA/applications/appimage-activator.desktop"
FILE_ACTIVATOR_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-activator.svg"

# The pre-rename entry, its icon, and its record are on disk before the install,
# and the pre-rename entry is the current default for every AppImage MIME type.
FILE_LEGACY_ENTRY="$DIRECTORY_DATA/applications/appimage-handler.desktop"
FILE_LEGACY_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-handler.svg"
FILE_LEGACY_MANIFEST="$DIRECTORY_DATA/gnome-appimage-integration/appimage-handler.manifest"
printf '[Desktop Entry]\nType=Application\nName=AppImage Handler\nExec=/bin/true handle %%f\n' \
    > "$FILE_LEGACY_ENTRY"
mkdir -p "$(dirname -- "$FILE_LEGACY_ICON")"
printf 'old-icon' > "$FILE_LEGACY_ICON"
mkdir -p "$(dirname -- "$FILE_LEGACY_MANIFEST")"
printf 'previous_default=application/vnd.appimage\tappimagelauncher.desktop\n' \
    > "$FILE_LEGACY_MANIFEST"
cat > "$DIRECTORY_XDG/home/.config/mimeapps.list" <<'LIST'
[Default Applications]
application/vnd.appimage=appimage-handler.desktop
application/x-appimage=appimage-handler.desktop
application/x-iso9660-appimage=appimage-handler.desktop
LIST

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$FILE_ACTIVATOR_ENTRY" ]; then
    fail_test "handler install did not write the activator entry"
fi
grep -q '^Name=AppImage Activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the right-click item is not named AppImage Activator"
grep -q '^Icon=appimage-activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the activator entry does not use the AppImage Activator icon"
grep -q '^StartupWMClass=appimage-activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the activator entry has no StartupWMClass, so the dock cannot match the window"
if [ ! -f "$FILE_ACTIVATOR_ICON" ]; then
    fail_test "handler install did not install the activator icon"
fi
grep -q 'appimage-activator' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "the AppImage MIME type does not use the activator icon"
if grep -q 'application-x-executable' "$DIRECTORY_DATA/mime/packages/appimage.xml"; then
    fail_test "the AppImage MIME type still uses the generic icon"
fi

# The rename must leave nothing behind that still answers for the MIME types.
if [ -f "$FILE_LEGACY_ENTRY" ]; then
    fail_test "handler install left the pre-rename entry in place"
fi
if [ -f "$FILE_LEGACY_ICON" ]; then
    fail_test "handler install left the pre-rename icon in place"
fi
if [ -f "$FILE_LEGACY_MANIFEST" ]; then
    fail_test "handler install left the pre-rename record in place"
fi
if [ ! -f "$DIRECTORY_DATA/gnome-appimage-integration/appimage-activator.manifest" ]; then
    fail_test "handler install did not write the activator record"
fi
# The re-created handler is now the default for every AppImage MIME type.
if [ "$(run_in_sandbox xdg-mime query default application/vnd.appimage)" != "appimage-activator.desktop" ]; then
    fail_test "the activator is not the default handler after the rename"
fi
grep -q 'pre-rename' "$DIRECTORY_TEMP/install.txt" \
    || fail_test "handler install did not report the files it migrated"
# The previous default is carried across the rename, so uninstall can restore it.
grep -q 'previous_default=application/vnd.appimage	appimagelauncher.desktop' \
    "$DIRECTORY_DATA/gnome-appimage-integration/appimage-activator.manifest" \
    || fail_test "the rename lost the recorded previous default"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler uninstall > "$DIRECTORY_TEMP/uninstall.txt" 2>&1
if [ -f "$FILE_ACTIVATOR_ENTRY" ]; then
    fail_test "handler uninstall left the activator entry"
fi
if [ -f "$FILE_ACTIVATOR_ICON" ]; then
    fail_test "handler uninstall left the activator icon"
fi
grep -q 'application-x-executable' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "handler uninstall did not restore the original MIME icon"

if ! python3 -c "import gi; gi.require_version('Gtk','4.0'); from gi.repository import Gtk" 2>/dev/null; then
    echo "SKIP: GTK4 Python bindings are not available (tool-gated)"
    pass_test "graphical activator (script only)"
    exit 0
fi

pass_test "graphical activator"
