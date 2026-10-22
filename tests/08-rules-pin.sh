#!/bin/sh
#
# Rules-pin test: this project still matches the shared-rules version it pins.
#
# Tier: repository state. Unlike most tests here it reads this repository rather
# than a scratch fixture, because the claim under test is precisely about this
# repository's own files.
#
# The shared rules live in 00-project-rules. This project carries a copy of one
# tagged version, recorded in rules.lock, and scripts/rules-check.sh (itself
# vendored) verifies that the copy is unmodified. A rule change belongs in
# 00-project-rules, followed by a version bump and a recorded upgrade here --
# not in an edit to a file this test covers.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)

fail() {
    echo "08-rules-pin: $1" >&2
    exit 1
}

FILE_LOCK="$REPOSITORY_ROOT/rules.lock"
SCRIPT_CHECK="$REPOSITORY_ROOT/scripts/rules-check.sh"

# 1. The pin exists, and so does the gate. Without the gate a lock is a claim
#    nothing checks.
[ -f "$FILE_LOCK" ] || fail 'no rules.lock: this project does not pin a shared-rules version'
[ -f "$SCRIPT_CHECK" ] || fail 'no scripts/rules-check.sh: the gate is missing, so drift cannot be detected'

# 2. The pin is a version of the shape the shared-rules repository issues. A
#    project tag uses <date>-<branch>; a rules version uses <date>.<counter>, so
#    the two are never mistaken for each other.
version=$(sed -n 's/^version = //p' "$FILE_LOCK" | head -n 1)
[ -n "$version" ] || fail 'rules.lock declares no version'
printf '%s\n' "$version" | grep -Eq '^[0-9]{4}-[0-9]{2}-[0-9]{2}\.[1-9][0-9]*$' \
    || fail "rules.lock version is not <date>.<counter>: $version"

# 3. The gate agrees, and confirms the version it checked.
output=$(sh "$SCRIPT_CHECK" "$REPOSITORY_ROOT" </dev/null) \
    || fail "rules-check failed: $output"
printf '%s\n' "$output" | grep -qF "ok (v$version" \
    || fail "rules-check did not confirm v$version: $output"

echo "08-rules-pin: ok ($version)"
exit 0
