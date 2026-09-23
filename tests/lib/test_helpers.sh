#!/bin/sh
#
# test_helpers.sh: shared definitions for the tests in tests/.
# This file is sourced, not executed; tests-run.sh only executes tests/*.sh.
set -eu

REPOSITORY_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"

build_program() {
    sh "$REPOSITORY_ROOT/scripts/program-build.sh" >/dev/null
}

fail_test() {
    echo "FAIL: $1" >&2
    exit 1
}

pass_test() {
    echo "PASS: $1"
}

# Build a synthetic type-2 AppImage: a host ELF, the AppImage magic at offset 8,
# and an appended SquashFS payload. Requires mksquashfs, od, dd, and a 64-bit
# little-endian ELF.
# Usage: build_synthetic_appimage <elf> <payload-dir> <output> [compression] [prebuilt-squashfs]
# A prebuilt SquashFS is appended as it is, so a test can hash exactly the bytes it
# appended (the signature covers the payload as written).
build_synthetic_appimage() {
    file_elf=$1
    directory_payload=$2
    file_output=$3
    compression_image=${4:-gzip}
    prebuilt_squashfs=${5:-}
    file_squashfs="$file_output.squashfs"

    if [ -n "$prebuilt_squashfs" ]; then
        file_squashfs="$prebuilt_squashfs"
    else
        mksquashfs "$directory_payload" "$file_squashfs" -comp "$compression_image" -noappend -quiet >/dev/null
    fi

    offset_section_headers=$(od -An -tu8 -j 40 -N 8 "$file_elf" | tr -d ' \n')
    size_section_header=$(od -An -tu2 -j 58 -N 2 "$file_elf" | tr -d ' \n')
    count_section_headers=$(od -An -tu2 -j 60 -N 2 "$file_elf" | tr -d ' \n')
    offset_last_header=$((offset_section_headers + size_section_header * (count_section_headers - 1)))
    end_last_section=$(( $(od -An -tu8 -j $((offset_last_header + 24)) -N 8 "$file_elf" | tr -d ' \n') + $(od -An -tu8 -j $((offset_last_header + 32)) -N 8 "$file_elf" | tr -d ' \n') ))
    end_section_table=$((offset_section_headers + size_section_header * count_section_headers))
    if [ "$end_section_table" -gt "$end_last_section" ]; then
        size_elf=$end_section_table
    else
        size_elf=$end_last_section
    fi

    cp "$file_elf" "$file_output"
    printf '\101\111\002' | dd of="$file_output" bs=1 seek=8 conv=notrunc 2>/dev/null
    size_current=$(wc -c < "$file_output")
    if [ "$size_current" -lt "$size_elf" ]; then
        dd if=/dev/zero bs=1 count=$((size_elf - size_current)) >> "$file_output" 2>/dev/null
    fi
    cat "$file_squashfs" >> "$file_output"
    if [ -z "$prebuilt_squashfs" ]; then
        rm -f "$file_squashfs"
    fi
}
