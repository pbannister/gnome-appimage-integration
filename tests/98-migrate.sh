#!/bin/sh
#
# Tool-gated test: `migrate` adopts a launcher another tool wrote.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.  Everything
# happens under a temporary $HOME; the real desktop is untouched.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "migrate (no mksquashfs)"
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
    pass_test "migrate (no host ELF)"
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
Exec=probe %U
Icon=probe
Categories=Utility;
X-AppImage-Version=4.2.0
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

# A launcher some other tool wrote: it runs the AppImage, and it carries a class.  The
# AppImage sits in the managed directory already, which is where AppImageLauncher put
# the ones it integrated, so migrating it does not move the file.
FILE_MANAGED="$DIRECTORY_XDG/home/Applications/Probe.AppImage"
mkdir -p "$DIRECTORY_XDG/home/Applications"
cp "$DIRECTORY_TEMP/Probe.AppImage" "$FILE_MANAGED"
FILE_LEGACY="$DIRECTORY_APPLICATIONS/legacy.Probe.desktop"
cat > "$FILE_LEGACY" <<ENTRY
[Desktop Entry]
Type=Application
Name=Probe App
Exec=$FILE_MANAGED %U
Icon=probe
StartupWMClass=LegacyClass
Categories=Utility;
ENTRY

echo "=== a dry run adopts nothing ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate --dry-run "$FILE_LEGACY" \
    > "$DIRECTORY_TEMP/dry.txt" 2>&1
grep -q 'runs:    ' "$DIRECTORY_TEMP/dry.txt" || fail_test "the dry run did not name the program"
grep -q 'dry run: nothing was written' "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not say it wrote nothing"
if [ -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "the dry run installed a launcher"
fi

echo "=== migrating by desktop file ID adopts the launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate legacy.Probe.desktop --yes \
    > "$DIRECTORY_TEMP/migrate.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "migrate did not install this tool's launcher: $(cat "$DIRECTORY_TEMP/migrate.txt")"
fi
if [ -f "$FILE_LEGACY" ]; then
    fail_test "migrate left the launcher it adopted in place"
fi
ls "$DIRECTORY_STATE/backup/" | grep -q 'legacy.Probe.desktop' \
    || fail_test "migrate did not back up the launcher it displaced"
grep -q '^X-Integrated-By=gnome-appimage-integration$' \
    "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the new launcher is not recorded as this tool's"
# The class comes from the launcher being replaced: the embedded entry names none.
grep -q '^StartupWMClass=LegacyClass$' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the class of the migrated launcher was not adopted"
grep -q '^displaced: ' "$DIRECTORY_TEMP/migrate.txt" \
    || fail_test "migrate did not say which launcher it displaced"

echo "=== the migration is reversible ==="
FILE_IDENTIFIER=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}')
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$FILE_IDENTIFIER" \
    > /dev/null 2>&1
if [ ! -f "$FILE_LEGACY" ]; then
    fail_test "uninstall did not restore the migrated launcher"
fi
if [ -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "uninstall left this tool's launcher behind"
fi

echo "=== migrating this tool's own launcher is a no-op ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate "$FILE_LEGACY" --yes \
    > "$DIRECTORY_TEMP/adopt.txt" 2>&1
FILE_MIGRATED="$DIRECTORY_APPLICATIONS/org.example.Probe.desktop"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate "$FILE_MIGRATED" --yes \
    > "$DIRECTORY_TEMP/ours.txt" 2>&1
grep -q 'nothing to migrate' "$DIRECTORY_TEMP/ours.txt" \
    || fail_test "migrating our own launcher is not reported as a no-op"
if [ ! -f "$FILE_MIGRATED" ]; then
    fail_test "the no-op removed our launcher"
fi

echo "=== a launcher whose AppImage is gone is refused ==="
cat > "$DIRECTORY_APPLICATIONS/legacy.Gone.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Gone App
Exec=/tmp/definitely-not-an-appimage/Thing.AppImage %U
Icon=thing
StartupWMClass=Thing
ENTRY
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate \
        "$DIRECTORY_APPLICATIONS/legacy.Gone.desktop" --yes > "$DIRECTORY_TEMP/gone.txt" 2>&1; then
    fail_test "migrating a launcher with no AppImage succeeded"
fi
grep -q 'which is not there' "$DIRECTORY_TEMP/gone.txt" \
    || fail_test "the refusal does not say the AppImage is missing"

echo "=== migrate --json reports the outcome ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" migrate --json \
    "$DIRECTORY_APPLICATIONS/legacy.Gone.desktop" > "$DIRECTORY_TEMP/gone.json" 2>&1 || true
python3 - "$DIRECTORY_TEMP/gone.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["migrated"] is False, data
assert "not there" in data["problem"], data
assert data["written_by_this_tool"] is False, data
print("migrate json ok")
PYTHON

pass_test "migrate"
