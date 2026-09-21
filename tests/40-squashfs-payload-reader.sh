#!/bin/sh
#
# Tool-gated test: SquashFS payload reader unit tests.
# Prerequisite: mksquashfs (squashfs-tools). Skips cleanly when it is absent.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1; then
    echo "SKIP: mksquashfs is not available (tool-gated)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_SOURCE="$DIRECTORY_TEMP/source"
mkdir -p "$DIRECTORY_SOURCE/usr/share/icons/hicolor/256x256/apps"
printf '[Desktop Entry]\nType=Application\nName=Test\nExec=test %%F\n' > "$DIRECTORY_SOURCE/test.desktop"
printf 'icon-bytes-0123456789' > "$DIRECTORY_SOURCE/.DirIcon"
printf 'png-bytes-0123456789' > "$DIRECTORY_SOURCE/usr/share/icons/hicolor/256x256/apps/fooview.png"
yes 'pattern-0123456789' 2>/dev/null | head -c 192000 > "$DIRECTORY_SOURCE/large.bin"
ln -s test.desktop "$DIRECTORY_SOURCE/link.desktop"

for compression_image in gzip xz zstd; do
    FILE_IMAGE="$DIRECTORY_TEMP/payload-$compression_image.squashfs"
    mksquashfs "$DIRECTORY_SOURCE" "$FILE_IMAGE" -comp "$compression_image" -noappend -quiet >/dev/null
    if ! "$DIRECTORY_BUILD/squashfs-reader-test" "$FILE_IMAGE" "$DIRECTORY_SOURCE" "$compression_image"; then
        fail_test "squashfs payload reader ($compression_image)"
    fi
done

FILE_IMAGE="$DIRECTORY_TEMP/payload-uncompressed.squashfs"
mksquashfs "$DIRECTORY_SOURCE" "$FILE_IMAGE" -noI -noD -noF -noX -noappend -quiet >/dev/null
if ! "$DIRECTORY_BUILD/squashfs-reader-test" "$FILE_IMAGE" "$DIRECTORY_SOURCE" "gzip"; then
    fail_test "squashfs payload reader (uncompressed blocks)"
fi

pass_test "squashfs payload reader"
