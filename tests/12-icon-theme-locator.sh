#!/bin/sh
#
# Portable test: icon theme locator unit tests.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

if ! "$DIRECTORY_BUILD/icon-theme-locator-test" "$DIRECTORY_TEMP"; then
    fail_test "icon theme locator unit tests"
fi

pass_test "icon theme locator"
