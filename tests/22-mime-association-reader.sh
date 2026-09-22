#!/bin/sh
#
# Portable test: MIME association reader unit tests.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

if ! "$DIRECTORY_BUILD/mime-association-reader-test" "$DIRECTORY_TEMP"; then
    fail_test "mime association reader unit tests"
fi

pass_test "mime association reader"
