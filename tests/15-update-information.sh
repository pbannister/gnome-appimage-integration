#!/bin/sh
#
# Portable test: the version ordering used to decide whether an installed AppImage is
# older than the newest release, and the reader for the update-information value.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

if ! "$DIRECTORY_BUILD/version-compare-test"; then
    fail_test "version comparison unit tests"
fi

if ! "$DIRECTORY_BUILD/appimage-update-information-test"; then
    fail_test "update information unit tests"
fi

pass_test "version comparison and update information"
