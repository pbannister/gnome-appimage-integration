#!/bin/sh
#
# Tool-gated test: conflict detection, replace-with-backup, and add-alongside.
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
# Everything happens under a temporary $HOME; the real desktop is untouched.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    exit 0
fi

FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done
if [ -z "$FILE_ELF" ] || [ "$(od -An -tu1 -j 4 -N 1 "$FILE_ELF" | tr -d ' \n')" != "2" ]; then
    echo "SKIP: no 64-bit host ELF executable found"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
cat > "$DIRECTORY_PAYLOAD/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
GenericName=Probe Tool
Comment=Probe comment
X-AppImage-Version=9.9.9
Exec=probe %U
Icon=probe
Categories=Utility;
StartupWMClass=ProbeApp
ENTRY

build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Probe.AppImage" gzip

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_APPLICATIONS="$DIRECTORY_XDG/home/.local/share/applications"
mkdir -p "$DIRECTORY_APPLICATIONS" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"
cp "$DIRECTORY_TEMP/Probe.AppImage" "$DIRECTORY_TEMP/work.AppImage"

run_in_sandbox() {
    env HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

# A launcher that already represents "Probe App", written by some other tool.
printf '[Desktop Entry]\nType=Application\nName=Probe App (1)\nExec=%s %%U\nIcon=probe\n' \
    "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop"

echo "=== plan must refuse and offer choices ==="
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/plan.txt" 2>&1; then
    fail_test "plan succeeded although a conflicting launcher exists"
fi
grep -q 'already represent' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not name the conflict"
grep -q -- '--replace' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not offer --replace"
grep -q -- '--add' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not offer --add"

echo "=== install --replace backs up the existing launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --replace --yes "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "replace did not install the new launcher"
fi
if [ -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "replace left the conflicting launcher in place"
fi
ls "$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/backup/" | grep -q 'legacy.Probe.desktop' \
    || fail_test "replace did not back up the conflicting launcher"

FILE_IDENTIFIER=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}')
echo "=== uninstall restores the replaced launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$FILE_IDENTIFIER" > /dev/null
if [ ! -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "uninstall did not restore the replaced launcher"
fi
if [ -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "uninstall left our launcher behind"
fi

echo "=== install --add keeps the existing launcher and adds a distinct one ==="
cp "$DIRECTORY_TEMP/Probe.AppImage" "$DIRECTORY_TEMP/work.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --add --yes "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/add.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "add removed the existing launcher"
fi
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe-2.desktop" ]; then
    fail_test "add did not create a distinct launcher"
fi

echo "=== explain shows the embedded entry ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain "$DIRECTORY_XDG/home/Applications/work.AppImage" > "$DIRECTORY_TEMP/explain.txt" 2>&1 || true
grep -q 'embedded desktop entry' "$DIRECTORY_TEMP/explain.txt" || fail_test "explain did not show the embedded entry"
grep -q 'Name=Probe App' "$DIRECTORY_TEMP/explain.txt" || fail_test "explain did not show the application name"

echo "=== explain --json carries the fields the graphical handler renders ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$DIRECTORY_XDG/home/Applications/work.AppImage" > "$DIRECTORY_TEMP/explain.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/explain.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["name"] == "Probe App", data["name"]
assert data["generic_name"] == "Probe Tool", data["generic_name"]
assert data["comment"] == "Probe comment", data["comment"]
assert data["version"] == "9.9.9", data["version"]
assert data["version_source"] == "X-AppImage-Version", data["version_source"]
assert isinstance(data["conflicts"], list)
print("json fields ok")
PYTHON

pass_test "integration conflicts"
