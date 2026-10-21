#!/bin/sh
#
# version-generate.sh: write the build-time version header.
# The version is a build-time fact derived from git state; see
# prompts/03-conventions.md section 6.2. The generated header is written to
# the build output directory and is never edited by hand.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"
DIRECTORY_GENERATED="$DIRECTORY_BUILD/generated"
FILE_HEADER="$DIRECTORY_GENERATED/version_info.h"
FILE_COUNTER="$DIRECTORY_BUILD/version-build-counter"

mkdir -p "$DIRECTORY_GENERATED"

DATE_BUILD=$(date +%Y-%m-%d)
BRANCH_BUILD=$(git -C "$REPOSITORY_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)
HASH_BUILD=$(git -C "$REPOSITORY_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)

# A tag is appended only when it names something other than a version this script already
# produces.  release-publish.sh invents v<version> when a commit has no tag, and that version
# is date-branch-hash, so embedding it would compound on the next build:
# date-branch-vdate-branch-hash-hash.  A human release tag, v2026.10.03, is not shaped like a
# version and is still appended.
SUFFIX_TAG=
if TAG_BUILD=$(git -C "$REPOSITORY_ROOT" describe --exact-match --tags 2>/dev/null); then
    case "$TAG_BUILD" in
        v[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]-*) ;;
        *) SUFFIX_TAG="-$TAG_BUILD" ;;
    esac
fi

SUFFIX_CHANGES=
if [ -n "$(git -C "$REPOSITORY_ROOT" status --porcelain 2>/dev/null)" ]; then
    SUFFIX_CHANGES=-changes
fi

VERSION_STRING="$DATE_BUILD-$BRANCH_BUILD$SUFFIX_TAG-$HASH_BUILD$SUFFIX_CHANGES"

COUNTER_BUILD=0
if [ -f "$FILE_COUNTER" ]; then
    COUNTER_BUILD=$(cat "$FILE_COUNTER")
fi
COUNTER_BUILD=$((COUNTER_BUILD + 1))
printf '%s\n' "$COUNTER_BUILD" > "$FILE_COUNTER"

{
    echo '/* Generated from git state by scripts/version-generate.sh - do not edit by hand. */'
    echo '#pragma once'
    printf '#define VERSION_STRING_VALUE "%s"\n' "$VERSION_STRING"
    printf '#define VERSION_BUILD_COUNTER_VALUE %s\n' "$COUNTER_BUILD"
} > "$FILE_HEADER"
