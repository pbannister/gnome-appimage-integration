#!/bin/sh
#
# Portable test: the minimal JSON value the graphical activator uses to read
# `appimage-integrate explain --json` and its own geometry file.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

if ! "$DIRECTORY_BUILD/json-reader-test"; then
    fail_test "json reader unit tests"
fi

pass_test "json reader"
