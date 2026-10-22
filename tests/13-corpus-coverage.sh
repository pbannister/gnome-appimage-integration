#!/bin/sh
#
# Corpus-coverage test: every validator check is exercised by the corpus.
#
# Tier: tool-gated (python3). tests/09-prompt-contract.sh declares each check
# with a numbered comment, `# <id>.`, and tests/10-prompt-validator.sh declares
# the check each rejection exercises. The two sets must match, so a new check
# cannot land without a fixture that exercises it, and a fixture cannot cite a
# check that does not exist.
#
# The check comment is the coverage unit. It proves a check is exercised, not
# that every branch inside it is; a branch that needs its own proof gets its own
# numbered check.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)

. "$DIRECTORY_SCRIPT/lib/test_helpers.sh"
skip_unless_tool python3

python3 - "$REPOSITORY_ROOT" <<'PY'
import os
import re
import sys

root = sys.argv[1]


def read(rel):
    with open(os.path.join(root, rel), encoding="utf-8") as handle:
        return handle.read()


validator = read("tests/09-prompt-contract.sh")
corpus = read("tests/10-prompt-validator.sh")

# `# <id>.` at the start of a comment line in the validator is a declared check.
declared = set(re.findall(r"^# (\d+(?:\.\d+)*)\.?\s", validator, re.M))
# `expect_reject <id> '<name>' ...` in the corpus names the check under test.
used = re.findall(r"^expect_reject (\d+(?:\.\d+)*) '", corpus, re.M)


def by_number(value):
    return tuple(int(part) for part in value.split("."))


failures = []
for check_id in sorted(declared - set(used), key=by_number):
    failures.append(f"check {check_id} has no fixture in tests/10; "
                    f"add 'expect_reject {check_id} ...'")
for check_id in sorted(set(used) - declared, key=by_number):
    failures.append(f"tests/10 cites check {check_id}, which tests/09 does not declare")

if failures:
    for failure in failures:
        print(f"13-corpus-coverage: {failure}", file=sys.stderr)
    sys.exit(1)

print(f"13-corpus-coverage: ok ({len(declared)} checks, "
      f"{len(used)} rejection cases)")
PY

exit 0
