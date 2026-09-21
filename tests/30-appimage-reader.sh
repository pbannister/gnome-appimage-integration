#!/bin/sh
#
# Portable test: AppImage container reader unit tests.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done

if [ -z "$FILE_ELF" ]; then
    echo "SKIP: no host ELF executable found"
    exit 0
fi

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

FILE_SQUASHFS=
if command -v mksquashfs >/dev/null 2>&1; then
    DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
    mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps"
    printf '[Desktop Entry]\nType=Application\nName=Test\nExec=test\n' > "$DIRECTORY_PAYLOAD/test.desktop"
    printf 'icon-bytes' > "$DIRECTORY_PAYLOAD/.DirIcon"
    printf 'png-bytes' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/256x256/apps/fooview.png"
    mksquashfs "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/payload.squashfs" -comp gzip -noappend -quiet >/dev/null
    FILE_SQUASHFS="$DIRECTORY_TEMP/payload.squashfs"
fi

if [ -n "$FILE_SQUASHFS" ]; then
    if ! "$DIRECTORY_BUILD/appimage-reader-test" "$FILE_ELF" "$DIRECTORY_TEMP" "$FILE_SQUASHFS"; then
        fail_test "appimage container reader"
    fi
else
    if ! "$DIRECTORY_BUILD/appimage-reader-test" "$FILE_ELF" "$DIRECTORY_TEMP"; then
        fail_test "appimage container reader"
    fi
fi

pass_test "appimage container reader"
