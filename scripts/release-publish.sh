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

# Build from the commit being released, so the products in the tarball are this commit's.
sh "$DIRECTORY_SCRIPT/program-build.sh"

VERSION=$("$REPOSITORY_ROOT/dataflow.out/build/appimage-integrate" --version | awk '{print $2}')
COMMIT=$(git -C "$REPOSITORY_ROOT" rev-parse --short HEAD)
HEAD_COMMIT=$(git -C "$REPOSITORY_ROOT" rev-parse HEAD)
case "$VERSION" in
    *"$COMMIT"*)
        ;;
    *)
        echo "release-publish: the build reports $VERSION, which does not name $COMMIT" >&2
        echo "release-publish: the products would not be the tagged commit's; stopping" >&2
        exit 1
        ;;
esac

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
    TAG_COMMIT=$(git -C "$REPOSITORY_ROOT" rev-parse "$TAG^{commit}")
    if [ "$TAG_COMMIT" != "$HEAD_COMMIT" ]; then
        echo "release-publish: tag $TAG is on $TAG_COMMIT, not on $HEAD_COMMIT" >&2
        echo "release-publish: a release has to match its tag; move the tag, or set TAG" >&2
        exit 1
    fi
    echo "release-publish: tag $TAG already exists on this commit; publishing the files to it"
else
    git -C "$REPOSITORY_ROOT" tag -a "$TAG" -m "$TITLE"
    echo "release-publish: tagged $TAG"
fi

# The commit and the tag first: a release whose tag is not pushed cannot be fetched.  A
# public repository can be read without credentials, so a push is skipped when the remote
# already has the ref, which is the usual case for a commit that has been pushed once.
push_if_needed() { # <ref> <description>
    if git -C "$REPOSITORY_ROOT" ls-remote --exit-code "$PUSH_REMOTE" "$1" > /dev/null 2>&1; then
        echo "release-publish: $PUSH_REMOTE already has $2"
        return 0
    fi
    if ! git -C "$REPOSITORY_ROOT" push "$PUSH_REMOTE" "$1"; then
        echo "release-publish: could not push $2 to $PUSH_REMOTE" >&2
        echo "release-publish: if this checkout pushes over SSH, set PUSH_REMOTE, for example:" >&2
        echo "release-publish:   PUSH_REMOTE=git@github.com:OWNER/REPO.git make release-publish" >&2
        return 1
    fi
    echo "release-publish: pushed $2"
    return 0
}

BRANCH=$(git -C "$REPOSITORY_ROOT" symbolic-ref --short HEAD)
push_if_needed "refs/heads/$BRANCH" "the branch $BRANCH" || exit 1
push_if_needed "refs/tags/$TAG" "the tag $TAG" || exit 1

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
