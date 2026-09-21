#!/bin/sh
#
# Test for the test-run log written by scripts/tests-run.sh.
# The runner mirrors its transcript into logs/YYYY-MM-DD-HH-MM-SS-test-run.log.
# This test checks that the newest such log exists, is non-empty, and already
# contains the banner of the currently running test.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
NAME_TEST=$(basename "$0")

FILE_LOG_NEWEST=$(ls -t "$REPOSITORY_ROOT"/logs/*-test-run.log 2>/dev/null | head -n 1)

if [ -z "$FILE_LOG_NEWEST" ]; then
    echo '02-test-run-log: no test-run log found in logs/' >&2
    exit 1
fi

PATTERN_LOG='[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]-test-run.log'

if ! printf '%s\n' "$FILE_LOG_NEWEST" | grep -q "$PATTERN_LOG$"; then
    echo "02-test-run-log: log filename is not timestamped: $FILE_LOG_NEWEST" >&2
    exit 1
fi

if [ ! -s "$FILE_LOG_NEWEST" ]; then
    echo "02-test-run-log: log is empty: $FILE_LOG_NEWEST" >&2
    exit 1
fi

if ! grep -q "=== $NAME_TEST" "$FILE_LOG_NEWEST"; then
    echo "02-test-run-log: log lacks the current test banner: $FILE_LOG_NEWEST" >&2
    exit 1
fi

echo '02-test-run-log: ok'
exit 0
