#!/bin/sh
#
# Test the build-time version string generation.
#
# The version is a build-time fact: date-branch-tag-hash, with -changes while the tree is
# dirty.  The subtlety is which tags may be appended.  release-publish.sh invents v<version>
# for a commit that has no tag, and a version is itself date-branch-hash, so appending that
# tag makes the next build's version embed the previous one:
#
#   2026-10-15-master-v2026-10-15-master-1a761f0-1a761f0
#
# and the one after that embeds that.  A human release tag, v2026.10.03, is what the form was
# for, and is still appended.
#
# The script derives its repository from its own location, so it is copied into a throwaway
# git repository and run there: nothing here touches the project's own tags.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v git >/dev/null 2>&1; then
    echo "SKIP: git is not available (tool-gated)"
    pass_test "version generation (no git)"
    exit 0
fi

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_REPOSITORY="$DIRECTORY_TEMP/repository"
mkdir -p "$DIRECTORY_REPOSITORY/scripts"
cp "$(dirname -- "$0")/../scripts/version-generate.sh" "$DIRECTORY_REPOSITORY/scripts/"

# A repository whose identity is fixed, so the test does not depend on the machine's git
# configuration.  The generated output is ignored, or writing it would dirty the tree and
# every version would carry -changes.
git -C "$DIRECTORY_REPOSITORY" init -q -b main
git -C "$DIRECTORY_REPOSITORY" config user.email "test@example.invalid"
git -C "$DIRECTORY_REPOSITORY" config user.name "Test"
git -C "$DIRECTORY_REPOSITORY" config commit.gpgsign false
printf 'dataflow.out/\n' > "$DIRECTORY_REPOSITORY/.gitignore"
printf 'one\n' > "$DIRECTORY_REPOSITORY/file.txt"
git -C "$DIRECTORY_REPOSITORY" add .gitignore file.txt scripts/version-generate.sh
git -C "$DIRECTORY_REPOSITORY" commit -q -m "first"

version_now() {
    sh "$DIRECTORY_REPOSITORY/scripts/version-generate.sh"
    sed -n 's/^#define VERSION_STRING_VALUE "\(.*\)"$/\1/p' \
        "$DIRECTORY_REPOSITORY/dataflow.out/build/generated/version_info.h"
}

HASH=$(git -C "$DIRECTORY_REPOSITORY" rev-parse --short HEAD)

echo "=== without a tag, the version is date-branch-hash ==="
base=$(version_now)
case "$base" in
    *-main-"$HASH") ;;
    *) fail_test "the version is not date-branch-hash: $base" ;;
esac
if ! printf '%s' "$base" | grep -qE '^[0-9]{4}-[0-9]{2}-[0-9]{2}-main-'; then
    fail_test "the version does not start with the build date and branch: $base"
fi

echo "=== a human release tag is part of the version ==="
git -C "$DIRECTORY_REPOSITORY" tag -a v2026.10.03 -m "a release"
version=$(version_now)
case "$version" in
    *-v2026.10.03-"$HASH") ;;
    *) fail_test "a release tag was not appended to the version: $version" ;;
esac
git -C "$DIRECTORY_REPOSITORY" tag -d v2026.10.03 >/dev/null

echo "=== a tag that is itself a version is not, so the versions do not compound ==="
# What release-publish.sh invents when a commit carries no tag.
git -C "$DIRECTORY_REPOSITORY" tag -a "v$base" -m "gnome-appimage-integration"
tagged=$(git -C "$DIRECTORY_REPOSITORY" describe --exact-match --tags)
if [ "$tagged" != "v$base" ]; then
    fail_test "the test did not tag the commit with the version: $tagged"
fi
version=$(version_now)
if [ "$version" != "$base" ]; then
    fail_test "the generated version was embedded again: $base became $version"
fi

echo "=== and neither does the next release, on a new commit ==="
# The order the release tool uses: compute the version at the commit, tag with it, and so
# make the *next* build the one that could embed it.
printf 'two\n' > "$DIRECTORY_REPOSITORY/file.txt"
git -C "$DIRECTORY_REPOSITORY" commit -q -am "second"
HASH=$(git -C "$DIRECTORY_REPOSITORY" rev-parse --short HEAD)
newbase=$(version_now)
case "$newbase" in
    *-main-"$HASH") ;;
    *) fail_test "the version on a new commit is not date-branch-hash: $newbase" ;;
esac
git -C "$DIRECTORY_REPOSITORY" tag -a "v$newbase" -m "gnome-appimage-integration"
second=$(version_now)
if [ "$second" != "$newbase" ]; then
    fail_test "the next release compounded the version: $newbase became $second"
fi

echo "=== a dirty tree says so ==="
printf 'three\n' >> "$DIRECTORY_REPOSITORY/file.txt"
dirty=$(version_now)
case "$dirty" in
    *-changes) ;;
    *) fail_test "an uncommitted change is not in the version: $dirty" ;;
esac

pass_test "version generation"
