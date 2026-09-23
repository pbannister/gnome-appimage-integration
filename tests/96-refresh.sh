#!/bin/sh
#
# Tool-gated test: `refresh` rewrites every recorded launcher from its embedded entry,
# so launchers written before a template change gain the new keys in one command.
# What the embedded entry does not carry is preserved from the launcher being replaced:
# the Name=, the desktop id, the icon name, and the window class.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.  Everything
# happens under a temporary $HOME; the real desktop is untouched.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "refresh (no mksquashfs)"
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
    pass_test "refresh (no host ELF)"
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
Exec=probe %U
Icon=probe
Categories=Utility;
ENTRY

build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Probe.AppImage" gzip

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_APPLICATIONS="$DIRECTORY_XDG/home/.local/share/applications"
DIRECTORY_STATE="$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration"
mkdir -p "$DIRECTORY_APPLICATIONS" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"

run_in_sandbox() {
    env HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

FILE_INSTALLED="$DIRECTORY_XDG/home/Applications/Probe.AppImage"

echo "=== install two launchers for one AppImage, with a chosen class ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    --wm-class ChosenClass "$DIRECTORY_TEMP/Probe.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes --add \
    --wm-class ChosenClass "$DIRECTORY_TEMP/Probe.AppImage" > "$DIRECTORY_TEMP/add.txt" 2>&1
FILE_ONE="$DIRECTORY_APPLICATIONS/org.example.Probe.desktop"
FILE_TWO="$DIRECTORY_APPLICATIONS/org.example.Probe-2.desktop"
if [ ! -f "$FILE_ONE" ] || [ ! -f "$FILE_TWO" ]; then
    fail_test "the two launchers were not installed"
fi
grep -q '^Actions=AppImage-Activator;Remove-AppImage;$' "$FILE_ONE" \
    || fail_test "the launcher has no context-menu actions"

echo "=== an older launcher is missing the new keys ==="
# Rewrite both launchers the way an earlier template did: no action, no class.
for FILE_LAUNCHER in "$FILE_ONE" "$FILE_TWO"; do
    grep -v -E '^(Actions=|StartupWMClass=|\[Desktop Action)' "$FILE_LAUNCHER" \
        > "$FILE_LAUNCHER.old"
    mv "$FILE_LAUNCHER.old" "$FILE_LAUNCHER"
done
if grep -q '^Actions=' "$FILE_ONE"; then
    fail_test "the launcher was not reduced to the old shape"
fi
if [ ! -f "$DIRECTORY_STATE/$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | head -1 | awk '{print $1}').manifest" ]; then
    fail_test "the record was not kept"
fi

echo "=== a dry run reports the launchers and writes nothing ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --dry-run \
    > "$DIRECTORY_TEMP/dry.txt" 2>&1
grep -q "launchers to rewrite: 2" "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not report both launchers"
grep -q "dry run: nothing was written" "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not say it wrote nothing"
if grep -q '^Actions=' "$FILE_ONE"; then
    fail_test "the dry run wrote to a launcher"
fi

echo "=== refresh rewrites both launchers, and does not call them a conflict ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes \
    > "$DIRECTORY_TEMP/refresh.txt" 2>&1
grep -q "rewritten launchers: 2 of 2" "$DIRECTORY_TEMP/refresh.txt" \
    || fail_test "refresh did not rewrite both launchers: $(cat "$DIRECTORY_TEMP/refresh.txt")"
for FILE_LAUNCHER in "$FILE_ONE" "$FILE_TWO"; do
    grep -q '^Actions=AppImage-Activator;Remove-AppImage;$' "$FILE_LAUNCHER" \
        || fail_test "refresh did not restore the context-menu actions in $FILE_LAUNCHER"
    grep -q '^StartupWMClass=ChosenClass$' "$FILE_LAUNCHER" \
        || fail_test "refresh did not restore the class in $FILE_LAUNCHER"
    grep -q '^\[Desktop Action AppImage-Activator\]$' "$FILE_LAUNCHER" \
        || fail_test "refresh did not write the action group in $FILE_LAUNCHER"
    grep -q "^Exec=$FILE_INSTALLED %U$" "$FILE_LAUNCHER" \
        || fail_test "refresh changed the target of $FILE_LAUNCHER"
done
if [ -f "$DIRECTORY_APPLICATIONS/org.example.Probe-3.desktop" ]; then
    fail_test "refresh created a third launcher"
fi

echo "=== without an option, the class in the launcher is kept ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes \
    > "$DIRECTORY_TEMP/keep.txt" 2>&1
grep -q '^StartupWMClass=ChosenClass$' "$FILE_ONE" \
    || fail_test "refresh replaced a class that was already set"

echo "=== an explicit class replaces it, and reaches every launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes --wm-class OtherClass \
    > "$DIRECTORY_TEMP/other.txt" 2>&1
for FILE_LAUNCHER in "$FILE_ONE" "$FILE_TWO"; do
    grep -q '^StartupWMClass=OtherClass$' "$FILE_LAUNCHER" \
        || fail_test "--wm-class did not reach $FILE_LAUNCHER"
done

echo "=== a class missing from a launcher is restored from the record ==="
grep -v '^StartupWMClass=' "$FILE_TWO" > "$FILE_TWO.old"
mv "$FILE_TWO.old" "$FILE_TWO"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes \
    > "$DIRECTORY_TEMP/restore.txt" 2>&1
grep -q '^StartupWMClass=OtherClass$' "$FILE_TWO" \
    || fail_test "refresh did not restore the class from the record"

echo "=== --wm-class-from-window falls back when nothing is running ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes --wm-class-from-window \
    > "$DIRECTORY_TEMP/window.txt" 2>&1
grep -q '^StartupWMClass=OtherClass$' "$FILE_ONE" \
    || fail_test "--wm-class-from-window lost the class of an AppImage that is not running"

echo "=== the name a user chose is preserved ==="
# The embedded entry says "Probe App"; the launcher says something else, as a user who
# renamed it would.  refresh must not put the embedded name back.
sed 's/^Name=Probe App$/Name=Probe App 9.9/' "$FILE_ONE" > "$FILE_ONE.renamed"
mv "$FILE_ONE.renamed" "$FILE_ONE"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes > /dev/null 2>&1
grep -q '^Name=Probe App 9.9$' "$FILE_ONE" \
    || fail_test "refresh lost the name the launcher carried"
grep -q '^Name=Probe App 9.9$' "$FILE_TWO" && \
    fail_test "refresh gave one launcher's name to another"

echo "=== refresh --json reports the launchers it wrote ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --json --yes \
    > "$DIRECTORY_TEMP/refresh.json" 2>&1
python3 - "$DIRECTORY_TEMP/refresh.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["written"] == 2, data
assert data["failed"] == [], data
assert data["skipped"] == [], data
entries = {entry["desktop_entry"]: entry for entry in data["launchers"]}
assert len(entries) == 2, data
for entry in entries.values():
    assert entry["startup_wm_class"], entry
    assert entry["mode"], entry
print("refresh json ok")
PYTHON

echo "=== a missing AppImage is reported and skipped ==="
mv "$FILE_INSTALLED" "$FILE_INSTALLED.gone"
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" refresh --yes \
        > "$DIRECTORY_TEMP/missing.txt" 2>&1; then
    fail_test "refresh succeeded although a recorded AppImage is gone"
fi
grep -q 'the AppImage is not at' "$DIRECTORY_TEMP/missing.txt" \
    || fail_test "refresh did not report the skipped launcher"
mv "$FILE_INSTALLED.gone" "$FILE_INSTALLED"

pass_test "refresh"
