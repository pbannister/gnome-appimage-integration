#!/bin/sh
#
# Integration test: command-line tools over a synthetic AppImage and desktop files.
# Prerequisite for the AppImage half: mksquashfs (squashfs-tools) and a 64-bit
# little-endian host ELF; that half skips cleanly when they are absent.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

FILE_DESKTOP_INSPECT="$DIRECTORY_BUILD/desktop-inspect"
FILE_APPIMAGE_INSPECT="$DIRECTORY_BUILD/appimage-inspect"

# --- desktop-inspect over the specification example ---
OUTPUT_TEXT=$("$FILE_DESKTOP_INSPECT" "$REPOSITORY_ROOT/dataflow.in/desktop/spec-example.desktop")
case "$OUTPUT_TEXT" in
    *'[Desktop Entry]'*) ;;
    *) fail_test "desktop-inspect did not print the Desktop Entry group" ;;
esac
case "$OUTPUT_TEXT" in
    *'Name=Foo Viewer'*) ;;
    *) fail_test "desktop-inspect did not print Name" ;;
esac

OUTPUT_LOCALIZED=$("$FILE_DESKTOP_INSPECT" --locale sr_YU "$REPOSITORY_ROOT/dataflow.in/desktop/localized.desktop")
case "$OUTPUT_LOCALIZED" in
    *'Name=Foo YU'*) ;;
    *) fail_test "desktop-inspect did not select the localized Name" ;;
esac

DIRECTORY_FIRST=$("$FILE_DESKTOP_INSPECT" --path | head -1)
if [ -z "$DIRECTORY_FIRST" ]; then
    fail_test "desktop-inspect --path printed nothing"
fi

OUTPUT_JSON=$("$FILE_DESKTOP_INSPECT" --json "$REPOSITORY_ROOT/dataflow.in/desktop/spec-example.desktop")
case "$OUTPUT_JSON" in
    '{'*'"groups"'*'}') ;;
    *) fail_test "desktop-inspect --json did not produce a JSON object" ;;
esac

"$FILE_DESKTOP_INSPECT" --version | grep -q 'desktop-inspect' \
    || fail_test "desktop-inspect --version printed nothing useful"

# --- appimage-inspect over a synthetic AppImage ---
FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done

if [ -z "$FILE_ELF" ] || ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: appimage-inspect integration needs mksquashfs, od, and a host ELF"
    pass_test "command-line tools (desktop half)"
    exit 0
fi

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

# A payload with one desktop entry, one icon, and one nested icon.
DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps"
printf '[Desktop Entry]\nType=Application\nName=Test\nExec=test %%F\nIcon=fooview\n' > "$DIRECTORY_PAYLOAD/test.desktop"
printf 'icon-bytes' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf 'png-bytes' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps/fooview.png"
mksquashfs "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/payload.squashfs" -comp gzip -noappend -quiet >/dev/null

# Compute the ELF size independently, using the documented algorithm.
uint1() { od -An -tu1 -j "$1" -N 1 "$2" | tr -d ' \n'; }
uint2() { od -An -tu2 -j "$1" -N 2 "$2" | tr -d ' \n'; }
uint8() { od -An -tu8 -j "$1" -N 8 "$2" | tr -d ' \n'; }

CLASS=$(uint1 4 "$FILE_ELF")
if [ "$CLASS" != "2" ]; then
    echo "SKIP: host ELF is not 64-bit"
    pass_test "command-line tools (desktop half)"
    exit 0
fi
SHOFF=$(uint8 40 "$FILE_ELF")
SHENTSIZE=$(uint2 58 "$FILE_ELF")
SHNUM=$(uint2 60 "$FILE_ELF")
LAST_HEADER=$((SHOFF + SHENTSIZE * (SHNUM - 1)))
LAST_SECTION_END=$(( $(uint8 $((LAST_HEADER + 24)) "$FILE_ELF") + $(uint8 $((LAST_HEADER + 32)) "$FILE_ELF") ))
TABLE_END=$((SHOFF + SHENTSIZE * SHNUM))
if [ "$TABLE_END" -gt "$LAST_SECTION_END" ]; then
    ELF_SIZE=$TABLE_END
else
    ELF_SIZE=$LAST_SECTION_END
fi

FILE_APPIMAGE="$DIRECTORY_TEMP/test.AppImage"
cp "$FILE_ELF" "$FILE_APPIMAGE"
printf '\101\111\002' | dd of="$FILE_APPIMAGE" bs=1 seek=8 conv=notrunc 2>/dev/null
SIZE_CURRENT=$(wc -c < "$FILE_APPIMAGE")
if [ "$SIZE_CURRENT" -lt "$ELF_SIZE" ]; then
    dd if=/dev/zero bs=1 count=$((ELF_SIZE - SIZE_CURRENT)) >> "$FILE_APPIMAGE" 2>/dev/null
fi
cat "$DIRECTORY_TEMP/payload.squashfs" >> "$FILE_APPIMAGE"

OUTPUT_CONTAINER=$("$FILE_APPIMAGE_INSPECT" "$FILE_APPIMAGE")
case "$OUTPUT_CONTAINER" in
    *'detection: type-2'*) ;;
    *) fail_test "appimage-inspect did not detect type 2" ;;
esac
case "$OUTPUT_CONTAINER" in
    *"payload-offset: $ELF_SIZE"*) ;;
    *) fail_test "appimage-inspect payload offset disagrees with the ELF size" ;;
esac
case "$OUTPUT_CONTAINER" in
    *'squashfs: present'*) ;;
    *) fail_test "appimage-inspect did not report the SquashFS payload" ;;
esac
case "$OUTPUT_CONTAINER" in
    *'embedded-desktop: /test.desktop'*) ;;
    *) fail_test "appimage-inspect did not find the embedded desktop entry" ;;
esac
case "$OUTPUT_CONTAINER" in
    *'Name=Test'*) ;;
    *) fail_test "appimage-inspect did not print the embedded desktop Name" ;;
esac

OUTPUT_DESKTOP=$("$FILE_APPIMAGE_INSPECT" --desktop "$FILE_APPIMAGE")
case "$OUTPUT_DESKTOP" in
    *'Name=Test'*) ;;
    *) fail_test "appimage-inspect --desktop did not print the embedded entry" ;;
esac

OUTPUT_LIST=$("$FILE_APPIMAGE_INSPECT" --list "$FILE_APPIMAGE")
case "$OUTPUT_LIST" in
    *'/usr/share/icons'*|*'directory'*) ;;
    *) fail_test "appimage-inspect --list printed no payload entries" ;;
esac

OUTPUT_JSON_APPIMAGE=$("$FILE_APPIMAGE_INSPECT" --json "$FILE_APPIMAGE")
case "$OUTPUT_JSON_APPIMAGE" in
    *'"detection":"type-2"'*) ;;
    *) fail_test "appimage-inspect --json did not report type 2" ;;
esac
case "$OUTPUT_JSON_APPIMAGE" in
    *'"present":true'*) ;;
    *) fail_test "appimage-inspect --json did not report the payload" ;;
esac

if "$FILE_APPIMAGE_INSPECT" "$REPOSITORY_ROOT/README.md" >/dev/null 2>&1; then
    fail_test "appimage-inspect accepted a non-ELF file"
fi

"$FILE_APPIMAGE_INSPECT" --version | grep -q 'appimage-inspect' \
    || fail_test "appimage-inspect --version printed nothing useful"

pass_test "command-line tools"
