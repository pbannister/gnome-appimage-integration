#!/bin/sh
#
# Tool-gated test: type 1 (ISO 9660) AppImage payload reading.
# Prerequisites: genisoimage or mkisofs, and a 64-bit little-endian host ELF.
# Skips cleanly when the tools are absent.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v genisoimage >/dev/null 2>&1 && ! command -v mkisofs >/dev/null 2>&1; then
    echo "SKIP: genisoimage/mkisofs is not available (tool-gated)"
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
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps"
printf '[Desktop Entry]\nType=Application\nName=TypeOne\nExec=AppRun %%U\nIcon=typeone\nStartupWMClass=TypeOne\n' \
    > "$DIRECTORY_PAYLOAD/typeone.desktop"
printf 'diricon-bytes-0123456789' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf 'png-bytes-0123456789' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps/typeone.png"
# A symlink in the payload: the AppDir shape the .DirIcon often takes.
mkdir -p "$DIRECTORY_PAYLOAD/share/applications"
ln -s share/applications "$DIRECTORY_PAYLOAD/sharealias"

build_type1_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/TypeOne.AppImage"

INSPECT_TEXT=$("$DIRECTORY_BUILD/appimage-inspect" "$DIRECTORY_TEMP/TypeOne.AppImage" 2>&1)
printf '%s\n' "$INSPECT_TEXT" | grep -q 'detection: type-1' \
    || fail_test "appimage-inspect did not detect the type 1 image"
printf '%s\n' "$INSPECT_TEXT" | grep -q 'typeone.desktop' \
    || fail_test "appimage-inspect did not list the root desktop entry"
printf '%s\n' "$INSPECT_TEXT" | grep -q 'embedded-desktop: /typeone.desktop' \
    || fail_test "appimage-inspect did not read the embedded desktop entry"

# plan reads the same payload through the ISO 9660 reader and finds the icon.
PLAN_TEXT=$("$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/TypeOne.AppImage" 2>&1)
printf '%s\n' "$PLAN_TEXT" | grep -q 'desktop-id: typeone.desktop' \
    || fail_test "plan did not read the embedded desktop entry"
printf '%s\n' "$PLAN_TEXT" | grep -q '256x256 .png' \
    || fail_test "plan did not find the themed icon"

pass_test "iso9660 payload reader"
