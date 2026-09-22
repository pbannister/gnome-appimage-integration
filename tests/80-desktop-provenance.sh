#!/bin/sh
#
# Portable test: the provenance commands (--icon, --mime, --explain) over a
# synthetic XDG tree, so the real desktop is not consulted.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_HOME="$DIRECTORY_TEMP/home"
DIRECTORY_DATA="$DIRECTORY_HOME/.local/share"
DIRECTORY_CONFIG="$DIRECTORY_HOME/.config"
mkdir -p "$DIRECTORY_DATA/applications" "$DIRECTORY_DATA/icons/hicolor/48x48/apps" "$DIRECTORY_CONFIG" "$DIRECTORY_TEMP/share" "$DIRECTORY_TEMP/etc"
printf 'fake-icon' > "$DIRECTORY_DATA/icons/hicolor/48x48/apps/probe-icon.png"
cat > "$DIRECTORY_DATA/applications/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe
Exec=probe
Icon=probe-icon
ENTRY
printf '[Default Applications]\napplication/x-probe=org.example.Probe.desktop;\n' > "$DIRECTORY_CONFIG/mimeapps.list"

run_in_sandbox() {
    env HOME="$DIRECTORY_HOME" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_TEMP/share" \
        XDG_CONFIG_HOME="$DIRECTORY_CONFIG" \
        XDG_CONFIG_DIRS="$DIRECTORY_TEMP/etc" \
        "$@"
}

FILE_ICON="$DIRECTORY_DATA/icons/hicolor/48x48/apps/probe-icon.png"

run_in_sandbox "$DIRECTORY_BUILD/desktop-inspect" --icon probe-icon > "$DIRECTORY_TEMP/icon.txt"
grep -q "best: $FILE_ICON" "$DIRECTORY_TEMP/icon.txt" || fail_test "--icon did not resolve the sandbox icon"

if run_in_sandbox "$DIRECTORY_BUILD/desktop-inspect" --icon missing-icon >/dev/null 2>&1; then
    fail_test "--icon accepted a missing icon"
fi

run_in_sandbox "$DIRECTORY_BUILD/desktop-inspect" --mime application/x-probe > "$DIRECTORY_TEMP/mime.txt"
grep -q 'default: org.example.Probe.desktop' "$DIRECTORY_TEMP/mime.txt" || fail_test "--mime did not resolve the sandbox default"
grep -q "source: $DIRECTORY_CONFIG/mimeapps.list" "$DIRECTORY_TEMP/mime.txt" || fail_test "--mime did not report the source file"

run_in_sandbox "$DIRECTORY_BUILD/desktop-inspect" --explain org.example.Probe.desktop > "$DIRECTORY_TEMP/explain.txt"
grep -q "winner: $DIRECTORY_DATA/applications/org.example.Probe.desktop" "$DIRECTORY_TEMP/explain.txt" \
    || fail_test "--explain did not report the winning entry"

run_in_sandbox "$DIRECTORY_BUILD/desktop-inspect" --path > "$DIRECTORY_TEMP/path.txt"
head -1 "$DIRECTORY_TEMP/path.txt" | grep -q "$DIRECTORY_DATA/applications" || fail_test "--path did not start with the user directory"

pass_test "desktop provenance commands"
