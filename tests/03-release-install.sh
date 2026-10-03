#!/bin/sh
#
# Tool-gated test: the release package and the script a user fetches from GitHub.
#
# `scripts/release-package.sh` makes the release assets from a built tree, and
# `scripts/install.sh` is what a user pipes into a shell: it downloads the tarball,
# checks it against SHA256SUMS, and installs it.  Both are exercised here against a
# local HTTP server, so nothing outside this machine is used.
#
# Prerequisites: tar, gzip, sha256sum, python3, and a built tree.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

for tool_name in tar gzip sha256sum python3; do
    if ! command -v "$tool_name" >/dev/null 2>&1; then
        echo "SKIP: $tool_name is not available (tool-gated)"
        pass_test "release install (no $tool_name)"
        exit 0
    fi
done

build_program

DIRECTORY_TEMP=$(mktemp -d)
PID_SERVER=
cleanup() {
    if [ -n "$PID_SERVER" ]; then
        kill "$PID_SERVER" 2>/dev/null || true
    fi
    rm -rf "$DIRECTORY_TEMP"
}
trap cleanup EXIT

PORT=8082
BASE_URL="http://127.0.0.1:$PORT"
DIRECTORY_RELEASE="$REPOSITORY_ROOT/dataflow.out/release"

echo "=== the release package is made from the build ==="
sh "$REPOSITORY_ROOT/scripts/release-package.sh" > "$DIRECTORY_TEMP/package.txt" 2>&1
FILE_TARBALL=$(ls "$DIRECTORY_RELEASE"/*.tar.gz)
FILE_ASSET=$(basename "$FILE_TARBALL")
if [ ! -f "$DIRECTORY_RELEASE/SHA256SUMS" ]; then
    fail_test "release-package.sh wrote no SHA256SUMS"
fi
if [ ! -f "$DIRECTORY_RELEASE/RELEASE-NOTES.md" ]; then
    fail_test "release-package.sh wrote no release notes"
fi
( cd "$DIRECTORY_RELEASE" && sha256sum -c SHA256SUMS > /dev/null ) \
    || fail_test "SHA256SUMS does not match the tarball it was made from"

echo "=== the tarball holds the products in the layout a prefix has ==="
tar -tzf "$FILE_TARBALL" > "$DIRECTORY_TEMP/listing.txt"
for file_expected in ./bin/appimage-inspect ./bin/desktop-inspect ./bin/appimage-integrate \
    ./share/icons/hicolor/scalable/apps/appimage-activator.svg ./VERSION ./README.md; do
    grep -qxF "$file_expected" "$DIRECTORY_TEMP/listing.txt" \
        || fail_test "the tarball has no $file_expected: $(cat "$DIRECTORY_TEMP/listing.txt")"
done
# The icon beside the tool as well as in the theme: beside the tool is where
# handler_icon_source looks first, and a tarball without it left `handler install`
# unable to install the handler's own icon at all.
grep -qxF './bin/icons/appimage-activator.svg' "$DIRECTORY_TEMP/listing.txt" \
    || fail_test "the tarball has no ./bin/icons/appimage-activator.svg: $(cat "$DIRECTORY_TEMP/listing.txt")"
if [ -x "$DIRECTORY_BUILD/appimage-activator" ]; then
    grep -qxF './bin/appimage-activator' "$DIRECTORY_TEMP/listing.txt" \
        || fail_test "the tarball has no activator although one was built"
fi

echo "=== the script installs from the release into a prefix ==="
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIRECTORY_RELEASE" \
    > "$DIRECTORY_TEMP/server.log" 2>&1 &
PID_SERVER=$!
sleep 1

# The installer registers the handler, so it must not touch the real desktop: every
# path it writes is under this sandbox.
DIRECTORY_HOME="$DIRECTORY_TEMP/home"
mkdir -p "$DIRECTORY_HOME/.config" "$DIRECTORY_TEMP/xdgshare"

run_installer() { # <log-file> [VAR=VALUE ...]
    file_log=$1
    shift
    env HOME="$DIRECTORY_HOME" \
        XDG_DATA_HOME="$DIRECTORY_HOME/.local/share" \
        XDG_CONFIG_HOME="$DIRECTORY_HOME/.config" \
        XDG_DATA_DIRS="$DIRECTORY_TEMP/xdgshare" \
        "$@" sh "$REPOSITORY_ROOT/scripts/install.sh" > "$file_log" 2>&1
}

DIRECTORY_PREFIX="$DIRECTORY_TEMP/prefix"
run_installer "$DIRECTORY_TEMP/install.txt" \
    PREFIX="$DIRECTORY_PREFIX" APPIMAGE_INTEGRATION_RELEASE_URL="$BASE_URL"
grep -q 'the download matches its published digest' "$DIRECTORY_TEMP/install.txt" \
    || fail_test "the installer did not verify the download: $(cat "$DIRECTORY_TEMP/install.txt")"
for program_name in appimage-inspect desktop-inspect appimage-integrate; do
    if [ ! -x "$DIRECTORY_PREFIX/bin/$program_name" ]; then
        fail_test "the installer did not install $program_name"
    fi
done
if [ ! -f "$DIRECTORY_PREFIX/share/icons/hicolor/scalable/apps/appimage-activator.svg" ]; then
    fail_test "the installer did not install the activator icon"
fi
if [ ! -f "$DIRECTORY_PREFIX/VERSION" ]; then
    fail_test "the installer did not install the version file"
fi
"$DIRECTORY_PREFIX/bin/appimage-integrate" --version > "$DIRECTORY_TEMP/version.txt" 2>&1 \
    || fail_test "the installed tool does not run"
grep -q 'appimage-integrate' "$DIRECTORY_TEMP/version.txt" \
    || fail_test "the installed tool reports no version: $(cat "$DIRECTORY_TEMP/version.txt")"

# The installer registers the handler rather than only saying how to.
if command -v xdg-mime >/dev/null 2>&1; then
    grep -qF 'the AppImage Activator is now the *.AppImage handler' "$DIRECTORY_TEMP/install.txt" \
        || fail_test "the installer did not report registering the handler: $(cat "$DIRECTORY_TEMP/install.txt")"
    FILE_HANDLER_ENTRY="$DIRECTORY_HOME/.local/share/applications/appimage-activator.desktop"
    if [ ! -f "$FILE_HANDLER_ENTRY" ]; then
        fail_test "the installer did not write the handler entry"
    fi
    # The handler's own icon, which needs the icon source the tarball must carry.
    if [ ! -f "$DIRECTORY_HOME/.local/share/icons/hicolor/scalable/apps/appimage-activator.svg" ]; then
        fail_test "the installer did not install the handler icon from the release layout"
    fi
    # A machine that never had an AppImage MIME definition gets one.
    FILE_MIME_DEFINITION="$DIRECTORY_HOME/.local/share/mime/packages/appimage.xml"
    if [ ! -f "$FILE_MIME_DEFINITION" ]; then
        fail_test "the installer wrote no AppImage MIME definition on a machine with none"
    fi
    grep -q 'type="application/vnd.appimage"' "$FILE_MIME_DEFINITION" \
        || fail_test "the MIME definition does not define the AppImage type"
    grep -q 'generic-icon name="appimage-activator"' "$FILE_MIME_DEFINITION" \
        || fail_test "the MIME definition does not point the file icon at the activator"
else
    echo "SKIP: xdg-mime is not available, so the handler step was not checked"
fi

echo "=== a download that does not match its digest is refused ==="
DIRECTORY_TAMPERED="$DIRECTORY_TEMP/tampered"
mkdir -p "$DIRECTORY_TAMPERED"
cp "$DIRECTORY_RELEASE/SHA256SUMS" "$DIRECTORY_TAMPERED/SHA256SUMS"
cp "$FILE_TARBALL" "$DIRECTORY_TAMPERED/$FILE_ASSET"
printf 'junk' >> "$DIRECTORY_TAMPERED/$FILE_ASSET"
kill "$PID_SERVER" 2>/dev/null || true
wait "$PID_SERVER" 2>/dev/null || true
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIRECTORY_TAMPERED" \
    > "$DIRECTORY_TEMP/server-tampered.log" 2>&1 &
PID_SERVER=$!
sleep 1
DIRECTORY_PREFIX_TWO="$DIRECTORY_TEMP/prefix-two"
if run_installer "$DIRECTORY_TEMP/tampered.txt" \
        PREFIX="$DIRECTORY_PREFIX_TWO" APPIMAGE_INTEGRATION_RELEASE_URL="$BASE_URL"; then
    fail_test "a download that does not match its digest was installed"
fi
grep -q 'does not match SHA256SUMS' "$DIRECTORY_TEMP/tampered.txt" \
    || fail_test "the refusal does not name the digest: $(cat "$DIRECTORY_TEMP/tampered.txt")"
if [ -e "$DIRECTORY_PREFIX_TWO/bin/appimage-integrate" ]; then
    fail_test "the tampered download was installed anyway"
fi

echo "=== no prebuilt build for this platform is said plainly ==="
# A release directory with no asset at all: without the build fallback, the installer
# has to say what is missing rather than fail obscurely or fetch something else.
DIRECTORY_EMPTY="$DIRECTORY_TEMP/empty"
mkdir -p "$DIRECTORY_EMPTY"
kill "$PID_SERVER" 2>/dev/null || true
wait "$PID_SERVER" 2>/dev/null || true
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIRECTORY_EMPTY" \
    > "$DIRECTORY_TEMP/server-empty.log" 2>&1 &
PID_SERVER=$!
sleep 1
if run_installer "$DIRECTORY_TEMP/empty.txt" \
        PREFIX="$DIRECTORY_TEMP/prefix-three" APPIMAGE_INTEGRATION_RELEASE_URL="$BASE_URL" \
        APPIMAGE_INTEGRATION_NO_BUILD=1; then
    fail_test "the installer succeeded although there was nothing to install"
fi
grep -q "there is no $FILE_ASSET in latest" "$DIRECTORY_TEMP/empty.txt" \
    || fail_test "the installer does not name the asset it looked for: $(cat "$DIRECTORY_TEMP/empty.txt")"

echo "=== a local tarball can be installed without a network ==="
DIRECTORY_PREFIX_FOUR="$DIRECTORY_TEMP/prefix-four"
DIRECTORY_HOME_FOUR="$DIRECTORY_TEMP/home-four"
mkdir -p "$DIRECTORY_HOME_FOUR/.config"
env HOME="$DIRECTORY_HOME_FOUR" XDG_DATA_HOME="$DIRECTORY_HOME_FOUR/.local/share" \
    XDG_CONFIG_HOME="$DIRECTORY_HOME_FOUR/.config" XDG_DATA_DIRS="$DIRECTORY_TEMP/xdgshare" \
    PREFIX="$DIRECTORY_PREFIX_FOUR" APPIMAGE_INTEGRATION_TARBALL="$FILE_TARBALL" \
    APPIMAGE_INTEGRATION_NO_HANDLER=1 \
    sh "$REPOSITORY_ROOT/scripts/install.sh" > "$DIRECTORY_TEMP/local.txt" 2>&1
if [ ! -x "$DIRECTORY_PREFIX_FOUR/bin/appimage-inspect" ]; then
    fail_test "the local tarball was not installed: $(cat "$DIRECTORY_TEMP/local.txt")"
fi
# The tools are installed and nothing is registered when the handler is declined.
grep -qF 'the *.AppImage handler was not registered, on request' "$DIRECTORY_TEMP/local.txt" \
    || fail_test "the installer did not honour APPIMAGE_INTEGRATION_NO_HANDLER: $(cat "$DIRECTORY_TEMP/local.txt")"
if [ -f "$DIRECTORY_HOME_FOUR/.local/share/applications/appimage-activator.desktop" ]; then
    fail_test "the installer registered the handler although APPIMAGE_INTEGRATION_NO_HANDLER=1"
fi

pass_test "release package and installer"
