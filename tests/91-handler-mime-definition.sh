#!/bin/sh
#
# Tool-gated test: the AppImage MIME definition `handler install` writes when the
# machine has none, and what `handler uninstall` does with it afterwards.
#
# A machine that never had an AppImage definition is what a fresh laptop is.  Most
# systems carry the two AppImage types in shared-mime-info, but nothing there points
# their generic icon at this tool, and a system whose shared-mime-info predates the
# types defines neither.  A definition under $XDG_DATA_HOME/mime outranks the system
# copy, so installing one is what makes the handler's icon and the file type right;
# nothing else in this project installs it.
#
# The real desktop is never touched: every path is under a temporary directory.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

skip_unless_tool xdg-mime

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_HOME="$DIRECTORY_TEMP/home"
DIRECTORY_DATA="$DIRECTORY_HOME/.local/share"
mkdir -p "$DIRECTORY_HOME/.config" "$DIRECTORY_TEMP/share"

run_in_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$DIRECTORY_HOME" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_TEMP/share" \
        XDG_CONFIG_HOME="$DIRECTORY_HOME/.config" \
        "$@"
}

FILE_DEFINITION="$DIRECTORY_DATA/mime/packages/appimage.xml"
FILE_ENTRY="$DIRECTORY_DATA/applications/appimage-activator.desktop"
FILE_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-activator.svg"
FILE_MANIFEST="$DIRECTORY_DATA/gnome-appimage-integration/appimage-activator.manifest"

if [ -e "$FILE_DEFINITION" ]; then
    fail_test "the sandbox already had an AppImage MIME definition"
fi

echo "=== handler install defines the AppImage types when the machine has none ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install \
    > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$FILE_DEFINITION" ]; then
    fail_test "handler install wrote no MIME definition: $(cat "$DIRECTORY_TEMP/install.txt")"
fi
grep -q 'type="application/vnd.appimage"' "$FILE_DEFINITION" \
    || fail_test "the definition does not define application/vnd.appimage"
grep -q 'type="application/x-iso9660-appimage"' "$FILE_DEFINITION" \
    || fail_test "the definition does not define application/x-iso9660-appimage"
grep -q 'generic-icon name="appimage-activator"' "$FILE_DEFINITION" \
    || fail_test "the definition does not point the file icon at the activator"
if [ ! -f "$FILE_ICON" ]; then
    fail_test "handler install did not install the handler icon"
fi
if [ ! -f "$FILE_ENTRY" ]; then
    fail_test "handler install did not write the handler entry"
fi
grep -q '^mime_created=1$' "$FILE_MANIFEST" \
    || fail_test "the record does not say the definition was this tool's: $(cat "$FILE_MANIFEST")"

echo "=== a second install keeps the definition recorded as this tool's ==="
# Nothing needs rewriting the second time, so the fact has to be carried forward from
# the record; without that the next uninstall would leave the definition behind.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install \
    > "$DIRECTORY_TEMP/install-two.txt" 2>&1
grep -q '^mime_package=' "$FILE_MANIFEST" \
    || fail_test "the second install stopped recording the definition"
grep -q '^mime_created=1$' "$FILE_MANIFEST" \
    || fail_test "the second install lost the fact that the definition is this tool's"

echo "=== handler uninstall removes the definition it created ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler uninstall \
    > "$DIRECTORY_TEMP/uninstall.txt" 2>&1
if [ -e "$FILE_DEFINITION" ]; then
    fail_test "handler uninstall left the definition it created behind"
fi
if [ -e "$FILE_ENTRY" ]; then
    fail_test "handler uninstall left the handler entry behind"
fi

echo "=== a definition that was already there is restored, not removed ==="
# The other machine's case: another tool, or an earlier install, defined the types.
# That definition is not ours to delete, so install backs it up and uninstall puts it
# back as it was -- including its own generic icon.
mkdir -p "$(dirname -- "$FILE_DEFINITION")"
cat > "$FILE_DEFINITION" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/vnd.appimage">
    <comment>AppImage application bundle (Type 2)</comment>
    <generic-icon name="application-x-executable"/>
    <glob pattern="*.AppImage"/>
  </mime-type>
</mime-info>
XML
cp "$FILE_DEFINITION" "$DIRECTORY_TEMP/original.xml"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install \
    > "$DIRECTORY_TEMP/install-three.txt" 2>&1
grep -q 'generic-icon name="appimage-activator"' "$FILE_DEFINITION" \
    || fail_test "handler install did not rewrite an existing definition's icon"
if grep -q '^mime_created=1$' "$FILE_MANIFEST"; then
    fail_test "the record claims a definition that was already there is this tool's"
fi
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler uninstall \
    > "$DIRECTORY_TEMP/uninstall-two.txt" 2>&1
if [ ! -f "$FILE_DEFINITION" ]; then
    fail_test "handler uninstall removed a definition that was not its own"
fi
if ! cmp -s "$DIRECTORY_TEMP/original.xml" "$FILE_DEFINITION"; then
    fail_test "handler uninstall did not restore the definition that was there before"
fi

pass_test "handler MIME definition"
