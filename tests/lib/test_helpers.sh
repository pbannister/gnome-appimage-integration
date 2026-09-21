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
