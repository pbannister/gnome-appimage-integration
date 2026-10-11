#!/bin/sh
#
# Tool-gated test: the live-state scripts.
#
# scripts/live-state-snapshot.sh writes a restore point, scripts/live-state-reset.sh
# returns the desktop to a clean start, and scripts/live-state-restore.sh puts the
# integrations back.  The real desktop is never touched: every path, including the
# restore point, is under a temporary directory.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "live state (no mksquashfs)"
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
    pass_test "live state (no host ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

# ---------------------------------------------------------------------------
# Two AppImages to integrate.

build_probe() { # <stem> <name> <extra-desktop-line>
    directory_payload="$DIRECTORY_TEMP/payload-$1"
    mkdir -p "$directory_payload/usr/share/icons/hicolor/48x48/apps"
    printf 'png-bytes' > "$directory_payload/usr/share/icons/hicolor/48x48/apps/$1.png"
    printf 'diricon' > "$directory_payload/.DirIcon"
    {
        printf '[Desktop Entry]\n'
        printf 'Type=Application\n'
        printf 'Name=%s\n' "$2"
        printf 'Exec=%s %%F\n' "$1"
        printf 'Icon=%s\n' "$1"
        printf 'MimeType=application/x-%s;\n' "$1"
        [ -z "$3" ] || printf '%s\n' "$3"
    } > "$directory_payload/$1.desktop"
    build_synthetic_appimage "$FILE_ELF" "$directory_payload" \
        "$DIRECTORY_TEMP/$1.AppImage" gzip
}

build_probe probe-one "Probe One" "StartupWMClass=ProbeOne"
build_probe probe-two "Probe Two" ""

# ---------------------------------------------------------------------------
# A sandbox desktop, with one file from another tool and one orphan from an earlier run
# of this one, to prove the reset is narrow.

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_CORPUS="$DIRECTORY_TEMP/work"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA/applications" "$DIRECTORY_DATA/mime/packages" \
    "$DIRECTORY_DATA/icons/hicolor/48x48/apps" "$DIRECTORY_XDG/home/.config" \
    "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"

printf '[Desktop Entry]\nType=Application\nName=Other Tool App\nExec=/bin/true\n' \
    > "$DIRECTORY_DATA/applications/other-tool.desktop"
printf '<mime-info/>' > "$DIRECTORY_DATA/mime/packages/appimage.xml"
printf '<mime-info/>' > "$DIRECTORY_DATA/mime/packages/appimagekit_deadbeef_other.xml"
printf 'png' > "$DIRECTORY_DATA/icons/hicolor/48x48/apps/appimage_other-tool.png"
printf '[Desktop Entry]\nType=Application\nName=Orphan\nExec=/bin/true\nX-Integrated-By=gnome-appimage-integration\n' \
    > "$DIRECTORY_DATA/applications/orphan.desktop"
printf '<mime-info/>' > "$DIRECTORY_DATA/mime/packages/0123456789abcdef_orphan.xml"
printf 'png' > "$DIRECTORY_DATA/icons/hicolor/48x48/apps/appimage_01234567_orphan.png"

run_sandboxed() {
    env -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        APPIMAGE_CORPUS_DIR="$DIRECTORY_CORPUS" \
        APPIMAGE_INTEGRATE="$DIRECTORY_BUILD/appimage-integrate" \
        "$@"
}

echo "=== two integrations, one with a chosen class and name ==="
run_sandboxed "$DIRECTORY_BUILD/appimage-integrate" install --yes --no-move \
    "$DIRECTORY_TEMP/probe-one.AppImage" > /dev/null
run_sandboxed "$DIRECTORY_BUILD/appimage-integrate" install --yes --no-move \
    --wm-class Chosen --name "Renamed App" "$DIRECTORY_TEMP/probe-two.AppImage" > /dev/null
if [ "$(run_sandboxed "$DIRECTORY_BUILD/appimage-integrate" list | wc -l)" != "2" ]; then
    fail_test "the two AppImages were not integrated"
fi

echo "=== the snapshot records what a restore needs ==="
run_sandboxed sh "$REPOSITORY_ROOT/scripts/live-state-snapshot.sh" > "$DIRECTORY_TEMP/snapshot.txt" 2>&1
FILE_RESTORE=$(ls -1t "$DIRECTORY_CORPUS"/live-state/restore-point-*.tsv | head -1)
if [ -z "$FILE_RESTORE" ] || [ ! -f "$FILE_RESTORE" ]; then
    fail_test "no restore point was written: $(cat "$DIRECTORY_TEMP/snapshot.txt")"
fi
ROWS=$(grep -vc '^#' "$FILE_RESTORE" || true)
if [ "$ROWS" != "2" ]; then
    fail_test "the restore point has $ROWS rows, not 2: $(cat "$FILE_RESTORE")"
fi
grep -q 'Renamed App' "$FILE_RESTORE" || fail_test "the restore point lost the chosen Name"
grep -q 'Chosen' "$FILE_RESTORE" || fail_test "the restore point lost the chosen class"

# The launchers as they stand, for comparison after the restore.
for file_entry in "$DIRECTORY_DATA"/applications/probe-*.desktop; do
    cp "$file_entry" "$DIRECTORY_TEMP/before-$(basename "$file_entry")"
done

echo "=== the reset removes this tool's state and nothing else ==="
run_sandboxed sh "$REPOSITORY_ROOT/scripts/live-state-reset.sh" > "$DIRECTORY_TEMP/reset.txt" 2>&1
if [ -n "$(run_sandboxed "$DIRECTORY_BUILD/appimage-integrate" list)" ]; then
    fail_test "the reset left an integration recorded: $(cat "$DIRECTORY_TEMP/reset.txt")"
fi
if [ -e "$DIRECTORY_DATA/applications/probe-one.desktop" ] \
    || [ -e "$DIRECTORY_DATA/applications/probe-two.desktop" ]; then
    fail_test "the reset left an integration launcher behind"
fi
if [ -e "$DIRECTORY_DATA/applications/orphan.desktop" ]; then
    fail_test "the reset left an orphaned launcher behind"
fi
if [ -e "$DIRECTORY_DATA/mime/packages/0123456789abcdef_orphan.xml" ]; then
    fail_test "the reset left an orphaned MIME definition behind"
fi
if [ -e "$DIRECTORY_DATA/icons/hicolor/48x48/apps/appimage_01234567_orphan.png" ]; then
    fail_test "the reset left an orphaned icon behind"
fi
# Another tool's files, and the shared definition, are not this tool's to remove.
for file_kept in "$DIRECTORY_DATA/applications/other-tool.desktop" \
    "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    "$DIRECTORY_DATA/mime/packages/appimagekit_deadbeef_other.xml" \
    "$DIRECTORY_DATA/icons/hicolor/48x48/apps/appimage_other-tool.png"; do
    if [ ! -e "$file_kept" ]; then
        fail_test "the reset removed a file that is not this tool's: $file_kept"
    fi
done
if ls "$DIRECTORY_DATA"/mime/packages/*_probe-*.xml > /dev/null 2>&1; then
    fail_test "the reset left an integration MIME definition behind"
fi

echo "=== the restore puts the integrations back as they were ==="
run_sandboxed sh "$REPOSITORY_ROOT/scripts/live-state-restore.sh" "$FILE_RESTORE" \
    > "$DIRECTORY_TEMP/restore.txt" 2>&1 \
    || fail_test "the restore failed: $(cat "$DIRECTORY_TEMP/restore.txt")"
if [ "$(run_sandboxed "$DIRECTORY_BUILD/appimage-integrate" list | wc -l)" != "2" ]; then
    fail_test "the restore did not put both integrations back"
fi
for file_before in "$DIRECTORY_TEMP"/before-*.desktop; do
    file_entry="$DIRECTORY_DATA/applications/$(basename "$file_before" | sed 's/^before-//')"
    if [ ! -f "$file_entry" ]; then
        fail_test "the restore did not write $file_entry"
    fi
    for key in Name Icon StartupWMClass; do
        expected=$(grep "^$key=" "$file_before" || true)
        actual=$(grep "^$key=" "$file_entry" || true)
        if [ "$expected" != "$actual" ]; then
            fail_test "$(basename "$file_entry"): $key is '$actual', was '$expected'"
        fi
    done
done
if [ -e "$DIRECTORY_DATA/applications/orphan.desktop" ]; then
    fail_test "the restore resurrected an orphaned launcher"
fi

pass_test "live state snapshot, reset, and restore"
