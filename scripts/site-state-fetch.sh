#!/bin/sh
#
# site-state-fetch.sh: capture this host's live state for the project pages.
#
# Convention: the homelab's documents/09-project-pages-conventions.md section 4.
# The site build substitutes __KEY__ placeholders in site.in/*.txt from the file
# written here; a key with no value renders as "unavailable", so the pages build
# anywhere and simply say less where the tool is absent.
#
# Output: dataflow.out/site-state.txt   KEY=VALUE lines, plus FETCHED (ISO 8601 UTC)
#
# Run this on the owning host: it reads the installed AppImages, the handler
# registration, and the audit of this desktop.  Values are sanitized of home paths
# (the publishing gate refuses them) and HTML-escaped, because they land in HTML.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"
FILE_STATE="$REPOSITORY_ROOT/dataflow.out/site-state.txt"
TOOL="$DIRECTORY_BUILD/appimage-integrate"

mkdir -p "$REPOSITORY_ROOT/dataflow.out"

# tidy: a home path becomes ~, and the characters that would break HTML are escaped.
tidy() {
    sed "s|$HOME|~|g" | sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'
}

# value_of: the whole output of a command, failures included, on one line.
value_of() {
    "$@" 2>/dev/null | tr '\n' ' ' | sed 's/  */ /g; s/ $//' || true
}

FETCHED=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
HOSTNAME_VALUE=$(uname -n 2>/dev/null || echo unknown)
RELEASE=$(git -C "$REPOSITORY_ROOT" describe --tags --abbrev=0 2>/dev/null || echo none)

VERSION=not-built
INTEGRATED_COUNT=0
MANAGED_DIR=unavailable
HANDLER=not-registered
AUDIT_ERRORS=0
AUDIT_WARNINGS=0

if [ -x "$TOOL" ]; then
    VERSION=$("$TOOL" --version 2>/dev/null | awk '{print $2}' || echo not-built)
    LISTING=$("$TOOL" list 2>/dev/null || true)
    INTEGRATED_COUNT=$(printf '%s\n' "$LISTING" | grep -c . || true)
    MANAGED_DIR=$(printf '%s\n' "$LISTING" | awk -F'\t' 'NF >= 2 { print $2 }' \
        | sed 's|/[^/]*$||' | sort | uniq -c | sort -rn | awk 'NR == 1 { print $2 }')
    [ -n "$MANAGED_DIR" ] || MANAGED_DIR=unavailable

    if "$TOOL" handler status 2>/dev/null | grep -q 'appimage-activator.desktop'; then
        HANDLER=registered
    fi

    AUDIT=$("$TOOL" audit 2>/dev/null || true)
    AUDIT_ERRORS=$(printf '%s\n' "$AUDIT" | grep -c '^error:' || true)
    AUDIT_WARNINGS=$(printf '%s\n' "$AUDIT" | grep -c '^warning:' || true)
fi

{
    echo "FETCHED=$FETCHED"
    echo "HOST=$(printf '%s' "$HOSTNAME_VALUE" | tidy)"
    echo "VERSION=$(printf '%s' "$VERSION" | tidy)"
    echo "RELEASE=$(printf '%s' "$RELEASE" | tidy)"
    echo "INTEGRATED_COUNT=$(printf '%s' "$INTEGRATED_COUNT" | tidy)"
    echo "MANAGED_DIR=$(printf '%s' "$MANAGED_DIR" | tidy)"
    echo "HANDLER=$(printf '%s' "$HANDLER" | tidy)"
    echo "AUDIT_ERRORS=$(printf '%s' "$AUDIT_ERRORS" | tidy)"
    echo "AUDIT_WARNINGS=$(printf '%s' "$AUDIT_WARNINGS" | tidy)"
} > "$FILE_STATE"

echo "site-state-fetch: wrote $FILE_STATE"
echo "site-state-fetch: version $VERSION on $HOSTNAME_VALUE, $INTEGRATED_COUNT AppImage(s) integrated, $AUDIT_ERRORS error(s) and $AUDIT_WARNINGS warning(s) in the audit"
exit 0
