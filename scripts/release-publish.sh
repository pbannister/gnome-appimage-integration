#!/bin/sh
#
# release-publish.sh: tag the current commit and publish the packaged files as a GitHub
# release.  Run it from a clean tree, after the work is committed.
#
#   make release            # or: sh scripts/release-publish.sh
#
# It needs the GitHub CLI, authenticated (`gh auth login`), because the release and its
# assets are made through the API.  Commits and the tag go to $PUSH_REMOTE (default
# "origin"); set PUSH_REMOTE to a URL when the configured remote cannot be pushed to.
#
# Environment:
#   TAG          the release tag (default v<version the products report>)
#   TITLE        the release title (default "gnome-appimage-integration <version>")
#   PUSH_REMOTE  where to push the commit and the tag (default origin)
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_RELEASE="$REPOSITORY_ROOT/dataflow.out/release"
PUSH_REMOTE=${PUSH_REMOTE:-origin}

if ! command -v gh > /dev/null 2>&1; then
    echo "release-publish: the GitHub CLI (gh) is needed; see https://cli.github.com" >&2
    exit 1
fi
if ! gh auth status > /dev/null 2>&1; then
    echo "release-publish: gh is not authenticated; run 'gh auth login', or set GH_TOKEN" >&2
    exit 1
fi

if [ -n "$(git -C "$REPOSITORY_ROOT" status --porcelain)" ]; then
    echo "release-publish: the working tree has changes; commit them first" >&2
    exit 1
fi

sh "$DIRECTORY_SCRIPT/release-package.sh"

VERSION=$(cat "$DIRECTORY_RELEASE/VERSION")
TAG=${TAG:-v$VERSION}
TITLE=${TITLE:-"gnome-appimage-integration $VERSION"}
FILE_TARBALL=$(find "$DIRECTORY_RELEASE" -maxdepth 1 -name '*.tar.gz' | head -1)
if [ -z "$FILE_TARBALL" ]; then
    echo "release-publish: release-package.sh produced no tarball" >&2
    exit 1
fi

if git -C "$REPOSITORY_ROOT" rev-parse --verify --quiet "refs/tags/$TAG" > /dev/null; then
    echo "release-publish: tag $TAG already exists; publishing the files to it"
else
    git -C "$REPOSITORY_ROOT" tag -a "$TAG" -m "$TITLE"
    echo "release-publish: tagged $TAG"
fi

# The commit and the tag first: a release whose tag is not pushed cannot be fetched.
git -C "$REPOSITORY_ROOT" push "$PUSH_REMOTE" HEAD
git -C "$REPOSITORY_ROOT" push "$PUSH_REMOTE" "$TAG"

if gh release view "$TAG" > /dev/null 2>&1; then
    echo "release-publish: release $TAG exists; uploading the files to it"
    gh release upload "$TAG" "$FILE_TARBALL" "$DIRECTORY_RELEASE/SHA256SUMS" --clobber
else
    gh release create "$TAG" \
        --title "$TITLE" \
        --notes-file "$DIRECTORY_RELEASE/RELEASE-NOTES.md" \
        "$FILE_TARBALL" "$DIRECTORY_RELEASE/SHA256SUMS"
fi

echo "release-publish: $TAG is published"
echo "release-publish: install it with"
echo "  curl -fsSL https://raw.githubusercontent.com/pbannister/gnome-appimage-integration/master/scripts/install.sh | sh"
