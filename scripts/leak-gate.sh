#!/bin/sh
#
# leak-gate.sh: refuse sensitive content in files or a generated tree.
#
# The gate runs before publishing (see documents/06-project-pages.md,
# "Sanitization"). The homelab re-runs its own gate at publish time; this is
# the project-side check so a leak is caught in the sandbox.
#
# The patterns live in scripts/sensitive-patterns.sh, their single source.
#
# Usage: leak-gate.sh PATH...
# Exits 0 when every path is clean; exits 1 and prints the first matches when
# a path is not.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$DIRECTORY_SCRIPT/sensitive-patterns.sh"

if [ "$#" -eq 0 ]; then
    echo 'leak-gate: usage: leak-gate.sh PATH...' >&2
    exit 2
fi

status_gate=0
for path_check in "$@"; do
    if [ ! -e "$path_check" ]; then
        echo "leak-gate: path not found: $path_check" >&2
        status_gate=1
        continue
    fi
    matches=$(grep -rEnI "$PATTERNS_SENSITIVE" "$path_check" 2>/dev/null || true)
    if [ -n "$matches" ]; then
        echo "leak-gate: REFUSING: sensitive content found in $path_check" >&2
        printf '%s\n' "$matches" | head -5 >&2
        status_gate=1
    fi
done

if [ "$status_gate" -ne 0 ]; then
    exit 1
fi

echo "leak-gate: clean: $*"
exit 0
