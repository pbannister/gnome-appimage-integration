#!/bin/sh
#
# Portable test: desktop entry reader unit tests.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

if ! "$DIRECTORY_BUILD/desktop-entry-reader-test" "$REPOSITORY_ROOT"; then
    fail_test "desktop entry reader unit tests"
fi

pass_test "desktop entry reader"
