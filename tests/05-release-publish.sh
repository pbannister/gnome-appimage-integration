#!/bin/sh
#
# Tool-gated test: release-publish.sh pushes the commit it is releasing.
#
# The remote having a ref is not the same as the remote having this commit.  A branch
# that exists there but is behind still has to be pushed, or the release is published
# while the branch the install script is fetched from -- `master/scripts/install.sh` --
# stays the old one, and a user who runs the documented one-liner gets the previous
# installer.  Publishing needs the GitHub CLI, so `gh` is stubbed and the "remote" is a
# local bare repository: nothing outside this machine is used.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

skip_unless_tool git

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

# A clean clone: release-publish refuses a dirty tree, and the real working tree must
# not be tagged or pushed by a test.
DIRECTORY_CLONE="$DIRECTORY_TEMP/clone"
BRANCH=$(git -C "$REPOSITORY_ROOT" symbolic-ref --short HEAD)
git clone --quiet --branch "$BRANCH" "$REPOSITORY_ROOT" "$DIRECTORY_CLONE"

# The stand-in remote already has the branch, one commit behind the release.
DIRECTORY_REMOTE="$DIRECTORY_TEMP/remote.git"
git init --quiet --bare "$DIRECTORY_REMOTE"
git -C "$DIRECTORY_CLONE" push --quiet "$DIRECTORY_REMOTE" "HEAD~1:refs/heads/$BRANCH"

# gh, stubbed: authenticated, no release yet, and recording what it is asked to create.
DIRECTORY_BIN="$DIRECTORY_TEMP/bin"
mkdir -p "$DIRECTORY_BIN"
cat > "$DIRECTORY_BIN/gh" <<'STUB'
#!/bin/sh
case "$1 $2" in
    "auth status") exit 0 ;;
    "release view") exit 1 ;;
    "release create")
        echo "gh release create $3" >> "$GH_RECORD"
        exit 0
        ;;
    "release upload")
        echo "gh release upload $3" >> "$GH_RECORD"
        exit 0
        ;;
esac
exit 0
STUB
chmod 755 "$DIRECTORY_BIN/gh"

TAG_RELEASE="v2026.10.02-test"
FILE_RECORD="$DIRECTORY_TEMP/gh.txt"
: > "$FILE_RECORD"

echo "=== publishing pushes the commit, not only the tag ==="
env PATH="$DIRECTORY_BIN:$PATH" GH_RECORD="$FILE_RECORD" \
    PUSH_REMOTE="$DIRECTORY_REMOTE" TAG="$TAG_RELEASE" \
    sh "$DIRECTORY_CLONE/scripts/release-publish.sh" > "$DIRECTORY_TEMP/publish.txt" 2>&1 \
    || fail_test "release-publish failed: $(cat "$DIRECTORY_TEMP/publish.txt")"

COMMIT_LOCAL=$(git -C "$DIRECTORY_CLONE" rev-parse "refs/heads/$BRANCH")
COMMIT_REMOTE=$(git -C "$DIRECTORY_REMOTE" rev-parse "refs/heads/$BRANCH")
if [ "$COMMIT_LOCAL" != "$COMMIT_REMOTE" ]; then
    fail_test "the branch on the remote is still $(git -C "$DIRECTORY_REMOTE" log -1 --format=%h "refs/heads/$BRANCH"), not the commit being released ($COMMIT_LOCAL): $(cat "$DIRECTORY_TEMP/publish.txt")"
fi
grep -q "pushed the branch $BRANCH" "$DIRECTORY_TEMP/publish.txt" \
    || fail_test "release-publish did not report pushing the branch: $(cat "$DIRECTORY_TEMP/publish.txt")"

echo "=== the tag is pushed and the release is created on it ==="
git -C "$DIRECTORY_REMOTE" rev-parse --verify --quiet "refs/tags/$TAG_RELEASE" > /dev/null \
    || fail_test "release-publish did not push the tag"
grep -q "gh release create $TAG_RELEASE" "$FILE_RECORD" \
    || fail_test "release-publish did not create the release: $(cat "$FILE_RECORD")"

echo "=== a second run finds nothing to push and uploads instead ==="
env PATH="$DIRECTORY_BIN:$PATH" GH_RECORD="$FILE_RECORD" \
    PUSH_REMOTE="$DIRECTORY_REMOTE" TAG="$TAG_RELEASE" \
    sh "$DIRECTORY_CLONE/scripts/release-publish.sh" > "$DIRECTORY_TEMP/publish-two.txt" 2>&1 \
    || fail_test "the second release-publish failed: $(cat "$DIRECTORY_TEMP/publish-two.txt")"
grep -q "already has the branch $BRANCH" "$DIRECTORY_TEMP/publish-two.txt" \
    || fail_test "the second run did not see the branch as already pushed: $(cat "$DIRECTORY_TEMP/publish-two.txt")"
grep -q "already has the tag $TAG_RELEASE" "$DIRECTORY_TEMP/publish-two.txt" \
    || fail_test "the second run did not see the tag as already pushed: $(cat "$DIRECTORY_TEMP/publish-two.txt")"

pass_test "release publish"
