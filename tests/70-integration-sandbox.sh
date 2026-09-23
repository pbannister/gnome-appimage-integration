#!/bin/sh
#
# Tool-gated test: plan, install, uninstall, and audit inside an isolated XDG sandbox.
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
# The real desktop is never touched; every path is under a temporary directory.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    exit 0
fi

FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done
if [ -z "$FILE_ELF" ]; then
    echo "SKIP: no host ELF executable found"
    exit 0
fi
if [ "$(od -An -tu1 -j 4 -N 1 "$FILE_ELF" | tr -d ' \n')" != "2" ]; then
    echo "SKIP: host ELF is not 64-bit"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/mime/packages"
printf 'fake-png-bytes' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/testapp.png"
printf '<svg xmlns="http://www.w3.org/2000/svg"/>' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/scalable/apps/testapp.svg"
printf 'fake-diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf '<?xml version="1.0"?><mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info"><mime-type type="application/x-test"><comment>Test</comment></mime-type></mime-info>' > "$DIRECTORY_PAYLOAD/usr/share/mime/packages/org.example.Test.xml"
cat > "$DIRECTORY_PAYLOAD/org.example.Test.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Test Application
GenericName=Integration Probe
Comment=Probe entry
Exec=TestApp %U
Icon=testapp
Categories=Utility;
MimeType=application/x-test;
StartupWMClass=TestApp
ENTRY

build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Test.AppImage" gzip
cp "$DIRECTORY_TEMP/Test.AppImage" "$DIRECTORY_TEMP/cli.AppImage"

if ! "$DIRECTORY_BUILD/integration-plan-test" "$DIRECTORY_TEMP/Test.AppImage" "$DIRECTORY_TEMP/plan-root"; then
    fail_test "integration plan, install, and uninstall"
fi

# The command-line tool in an isolated XDG home.
DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
mkdir -p "$DIRECTORY_XDG/home" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"

run_in_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

FILE_DESKTOP="$DIRECTORY_XDG/home/.local/share/applications/org.example.Test.desktop"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/cli.AppImage" > "$DIRECTORY_TEMP/plan.txt"
grep -q 'desktop-id: org.example.Test.desktop' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not report the desktop id"
if grep -q 'warning: the embedded entry has no StartupWMClass' "$DIRECTORY_TEMP/plan.txt"; then
    fail_test "plan warned about StartupWMClass although the entry sets it"
fi
grep -q 'StartupWMClass=TestApp' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan lost StartupWMClass"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" run --detached "$DIRECTORY_TEMP/cli.AppImage" > "$DIRECTORY_TEMP/detached.txt" 2>&1
grep -q 'Starting ' "$DIRECTORY_TEMP/detached.txt" || fail_test "run --detached did not report the start"
grep -q 'process ' "$DIRECTORY_TEMP/detached.txt" || fail_test "run --detached did not report the process id"

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$DIRECTORY_TEMP/cli.AppImage" > /dev/null
if [ ! -f "$FILE_DESKTOP" ]; then
    fail_test "install did not write the desktop entry"
fi
if [ ! -f "$DIRECTORY_XDG/home/Applications/cli.AppImage" ]; then
    fail_test "install did not move the AppImage to the managed directory"
fi
if [ ! -f "$DIRECTORY_XDG/home/.local/share/icons/hicolor/48x48/apps/appimage_testapp.png" ] \
    && ! ls "$DIRECTORY_XDG/home/.local/share/icons/hicolor/48x48/apps/" | grep -q '_testapp.png'; then
    fail_test "install did not write the 48x48 icon"
fi

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | grep -q 'org.example.Test.desktop' \
    || fail_test "list did not report the installed AppImage"

# A stale icon-theme cache hides new icons from GTK, so install must refresh it.
if command -v gtk4-update-icon-cache >/dev/null 2>&1 || command -v gtk-update-icon-cache >/dev/null 2>&1; then
    if [ ! -f "$DIRECTORY_XDG/home/.local/share/icons/hicolor/icon-theme.cache" ]; then
        fail_test "install did not refresh the icon theme cache"
    fi
fi

FILE_IDENTIFIER=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}')
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$FILE_IDENTIFIER" > /dev/null
if [ -f "$FILE_DESKTOP" ]; then
    fail_test "uninstall left the desktop entry behind"
fi

# Handler management is read-only in status mode and never touches the real home
# when the XDG variables are redirected.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler status | grep -q 'application/vnd.appimage' \
    || fail_test "handler status printed nothing useful"

pass_test "integration sandbox"
