#!/bin/sh
#
# tests-run.sh: run every test in tests/ in order; stop at the first failure.
# Invoked by `npm test`, which the top-level Makefile calls via `make test`.
# Every run mirrors its transcript into a timestamped log file:
#   logs/YYYY-MM-DD-HH-MM-SS-test-run.log
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)

mkdir -p "$REPOSITORY_ROOT/logs"
TIMESTAMP_RUN=$(date +%Y-%m-%d-%H-%M-%S)
FILE_LOG="$REPOSITORY_ROOT/logs/$TIMESTAMP_RUN-test-run.log"
echo "tests-run: log: $FILE_LOG" | tee -a "$FILE_LOG"

for file_test in "$REPOSITORY_ROOT"/tests/*.sh; do
    echo "=== $(basename "$file_test")" | tee -a "$FILE_LOG"
    if output_test=$(sh "$file_test" 2>&1); then
        status_test=0
    else
        status_test=$?
    fi
    if [ -n "$output_test" ]; then
        printf '%s\n' "$output_test" | tee -a "$FILE_LOG"
    fi
    if [ "$status_test" -ne 0 ]; then
        echo "tests-run: FAILED: $(basename "$file_test")" >&2
        exit 1
    fi
done

echo 'tests-run: all tests passed' | tee -a "$FILE_LOG"
exit 0
