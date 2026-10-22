#!/bin/sh
#
# Portable test: the SHA-256 implementation the AppImage signature check uses.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

if ! "$DIRECTORY_BUILD/sha256-test"; then
    fail_test "sha256 unit tests"
fi

# Cross-check against the system tool, so the implementation is not its own judge.
if command -v sha256sum >/dev/null 2>&1; then
    DIRECTORY_TEMP=$(mktemp -d)
    trap 'rm -rf "$DIRECTORY_TEMP"' EXIT
    printf 'abc' > "$DIRECTORY_TEMP/abc"
    EXPECTED=$(sha256sum "$DIRECTORY_TEMP/abc" | cut -d' ' -f1)
    if [ "$EXPECTED" != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ]; then
        fail_test "sha256sum disagrees with the published digest of abc"
    fi
fi

pass_test "sha256"
