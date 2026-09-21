#!/bin/sh
#
# Portable test: desktop entry locator unit tests.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

if ! "$DIRECTORY_BUILD/desktop-entry-locator-test" "$DIRECTORY_TEMP"; then
    fail_test "desktop entry locator unit tests"
fi

pass_test "desktop entry locator"
