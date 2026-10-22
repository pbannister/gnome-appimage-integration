#!/bin/sh
#
# rules-check.sh: verify that this project still matches the shared-rules
# version it pins, and that no local file restates shared rule text.
#
# Read-only. Never reads standard input. Exits 0 when the project is
# conformant, 1 when it is not, 2 on a usage error.
#
# Usage: rules-check.sh [PROJECT_DIR]
#
# The lock is rules.lock at the project root, written by
# 00-project-rules/tools/rules-vendor.sh. A locked file is byte-identical to
# the tagged version it came from. Editing one is drift, not customization:
# customize by extending (a local file that sorts after the shared one) or by
# recording a deviation in prompts/20-deviations.md.
#
# Comment style: one idea per paragraph. The check has four parts.
set -eu
LC_ALL=C
export LC_ALL

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "${1:-"$DIRECTORY_SCRIPT/.."}" && pwd)
FILE_LOCK="$PROJECT_ROOT/rules.lock"

fail() {
    echo "rules-check: $1" >&2
    echo 'rules-check: FAILED' >&2
    exit 1
}

note() {
    echo "rules-check: $1" >&2
}

# --- setup ----------------------------------------------------------------

if [ ! -f "$FILE_LOCK" ]; then
    fail "no rules.lock in $PROJECT_ROOT: this project does not pin shared rules (see 00-project-rules/README.md)"
fi

if command -v sha256sum >/dev/null 2>&1; then
    hash_file() { sha256sum -- "$1" | cut -d' ' -f1; }
elif command -v shasum >/dev/null 2>&1; then
    hash_file() { shasum -a 256 -- "$1" | cut -d' ' -f1; }
else
    fail 'no sha256sum or shasum on PATH'
fi

version=$(sed -n 's/^version = //p' "$FILE_LOCK" | head -n 1)
[ -n "$version" ] || fail 'rules.lock declares no version line'

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT HUP INT TERM

# A lock entry is "<64 hex>  <path>"; every other line is metadata or a comment.
awk 'length($1) == 64 && $2 != "" {print $1, $2}' "$FILE_LOCK" > "$DIRECTORY_TEMP/entries"
count_locked=$(wc -l < "$DIRECTORY_TEMP/entries" | tr -d ' ')
[ "$count_locked" -gt 0 ] || fail 'rules.lock lists no files'
awk '{print $2}' "$DIRECTORY_TEMP/entries" | sort > "$DIRECTORY_TEMP/locked"

# --- 1. every locked file is present and unmodified -----------------------

count_missing=0
count_drift=0
while read -r expected rel; do
    case "$rel" in
        /*|*..*|'') fail "rules.lock contains an unsafe path: $rel" ;;
    esac
    if [ ! -f "$PROJECT_ROOT/$rel" ]; then
        note "missing: $rel"
        count_missing=$((count_missing + 1))
        continue
    fi
    actual=$(hash_file "$PROJECT_ROOT/$rel")
    if [ "$actual" != "$expected" ]; then
        note "modified: $rel (a vendored rule is not editable; upgrade it through a task)"
        count_drift=$((count_drift + 1))
    fi
done < "$DIRECTORY_TEMP/entries"

# --- 2. no local rule file restates shared rule text ---------------------
#
# A local file may extend a shared rule, but the shared sentence stays in the
# shared file, so a version bump updates it in one place. Long lines are the
# signal: a line of 40 characters or more that appears in a shared rule file
# and again in a local one was copied, not written.
#
# Only rule-bearing local files are scanned. A task, an episode, and an eval
# case quote shared text on purpose -- a task that says "run `make test`" is
# restating nothing that belongs to a rule file -- so they are exempt.

: > "$DIRECTORY_TEMP/shared"
while read -r rel; do
    case "$rel" in
        prompts/*|evals/*)
            [ -f "$PROJECT_ROOT/$rel" ] || continue
            awk 'length($0) >= 40' "$PROJECT_ROOT/$rel" >> "$DIRECTORY_TEMP/shared"
            ;;
    esac
done < "$DIRECTORY_TEMP/locked"
sort -u -o "$DIRECTORY_TEMP/shared" "$DIRECTORY_TEMP/shared"

find "$PROJECT_ROOT/prompts" "$PROJECT_ROOT/evals" -type f 2>/dev/null \
    | sed "s#^$PROJECT_ROOT/##" | sort > "$DIRECTORY_TEMP/present" || true
comm -23 "$DIRECTORY_TEMP/present" "$DIRECTORY_TEMP/locked" > "$DIRECTORY_TEMP/local"

awk '
    /^prompts\/tasks\// { next }
    /^prompts\/episodes\// { next }
    /^evals\// { next }
    /\.md$/ { print }
' "$DIRECTORY_TEMP/local" > "$DIRECTORY_TEMP/local.rules"

: > "$DIRECTORY_TEMP/restated"
while read -r rel; do
    awk -v F="$rel" 'length($0) >= 40 {print $0 "\001" F}' "$PROJECT_ROOT/$rel" \
        >> "$DIRECTORY_TEMP/restated"
done < "$DIRECTORY_TEMP/local.rules"

count_restated=0
restated_report=$(awk -F'\001' '
    NR == FNR { share[$0]; next }
    ($1 in share) { print $2 ": restates shared rule text: " substr($1, 1, 58) "..." }
' "$DIRECTORY_TEMP/shared" "$DIRECTORY_TEMP/restated" | sort -u)
if [ -n "$restated_report" ]; then
    printf '%s\n' "$restated_report" | sed 's/^/rules-check: /' >&2
    count_restated=$(printf '%s\n' "$restated_report" | wc -l | tr -d ' ')
fi

# --- 3. every deviation still points at text that exists ------------------
#
# A deviation is a local override of one shared rule. Its quote is an anchor:
# the exact shared line it overrode. When a later version rewrites that line
# the anchor disappears and the deviation is stale, so the check fails and
# someone re-reviews the decision instead of inheriting it silently.
#
# Format (prompts/20-deviations.md):
#
#     ## DEVIATION: <short name>
#     - anchor: prompts/03-conventions.md
#     - quote: <one complete line, verbatim from the anchor>
#     - reason: <why this project differs>
#     - review: <YYYY-MM-DD>

FILE_DEVIATIONS="$PROJECT_ROOT/prompts/20-deviations.md"
count_deviation=0
count_anchor_fail=0
if [ -f "$FILE_DEVIATIONS" ]; then
    awk '
        /^## DEVIATION:/ { in_block = 1; anchor = ""; next }
        in_block && /^## / { in_block = 0; next }
        in_block && /^- anchor:/ { anchor = $0; sub(/^- anchor:[ \t]*/, "", anchor); next }
        in_block && /^- quote:/ {
            quote = $0
            sub(/^- quote:[ \t]*/, "", quote)
            if (anchor == "") { print "!\t<quote before anchor>"; exit }
            print anchor "\t" quote
            next
        }
    ' "$FILE_DEVIATIONS" > "$DIRECTORY_TEMP/deviations"

    while IFS="$(printf '\t')" read -r anchor quote; do
        [ -n "$anchor" ] || continue
        count_deviation=$((count_deviation + 1))
        case "$anchor" in
            /*|*..*) note "deviation anchors an unsafe path: $anchor"; count_anchor_fail=$((count_anchor_fail + 1)); continue ;;
        esac
        if ! grep -qxF -- "$anchor" "$DIRECTORY_TEMP/locked"; then
            note "deviation anchors $anchor, which is not a pinned shared file"
            count_anchor_fail=$((count_anchor_fail + 1))
            continue
        fi
        if ! grep -qxF -- "$quote" "$PROJECT_ROOT/$anchor"; then
            note "deviation anchor is stale: $anchor no longer contains the quoted line (re-review prompts/20-deviations.md)"
            count_anchor_fail=$((count_anchor_fail + 1))
        fi
    done < "$DIRECTORY_TEMP/deviations"
fi

# --- 4. report ------------------------------------------------------------

total=$((count_missing + count_drift + count_restated + count_anchor_fail))
if [ "$total" -ne 0 ]; then
    fail "$total problem(s): $count_missing missing, $count_drift modified, $count_restated restated, $count_anchor_fail stale deviation(s), at v$version"
fi

echo "rules-check: ok (v$version, $count_locked files, $count_deviation deviation(s))"
exit 0
