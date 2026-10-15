#!/bin/sh
#
# Tool-gated test: the AppImage specification's desktop-integration conditions.
#
# The specification says integration SHOULD NOT happen when one of five conditions holds.
# This project detects them and warns, and then does what it was asked: the warnings are
# shown, the install proceeds, and no button is disabled.  The unit test covers the
# detection itself, including the two paths no test can create under /usr or /etc; this
# covers the behaviour end to end.
#
# Prerequisites: mksquashfs, od, a C compiler, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "desktop integration opt-out (no mksquashfs)"
    exit 0
fi

FILE_COMPILER=$(command -v cc || command -v gcc || command -v c++ || true)
FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done
if [ -z "$FILE_COMPILER" ] || [ -z "$FILE_ELF" ] \
    || [ "$(od -An -tu1 -j 4 -N 1 "$FILE_ELF" | tr -d ' \n')" != "2" ]; then
    echo "SKIP: a C compiler and a 64-bit host ELF are needed"
    pass_test "desktop integration opt-out (no compiler or ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
PID_APPIMAGED=
cleanup() {
    if [ -n "$PID_APPIMAGED" ]; then
        kill "$PID_APPIMAGED" 2>/dev/null || true
    fi
    rm -rf "$DIRECTORY_TEMP"
}
trap cleanup EXIT

echo "=== the detection itself, every condition ==="
"$DIRECTORY_BUILD/desktop-integration-optout-test" \
    || fail_test "the desktop-integration opt-out unit test failed"

# One AppImage to integrate.
DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf '[Desktop Entry]\nType=Application\nName=Probe App\nExec=probe %%F\nIcon=probe\nStartupWMClass=ProbeApp\n' \
    > "$DIRECTORY_PAYLOAD/probe.desktop"
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/probe.AppImage" gzip

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA" "$DIRECTORY_XDG/home/.config" "$DIRECTORY_XDG/share" \
    "$DIRECTORY_XDG/etc"

run_in_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY -u DESKTOPINTEGRATION \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

echo "=== with nothing set, there is nothing to say ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/probe.AppImage" \
    > "$DIRECTORY_TEMP/plan-quiet.txt" 2>&1
if grep -q 'warning: desktop integration:' "$DIRECTORY_TEMP/plan-quiet.txt"; then
    fail_test "a warning was printed with no condition set: $(cat "$DIRECTORY_TEMP/plan-quiet.txt")"
fi

echo "=== the user-level sentinel: warned about, and installed anyway ==="
mkdir -p "$DIRECTORY_DATA/appimagekit"
: > "$DIRECTORY_DATA/appimagekit/no_desktopintegration"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/probe.AppImage" \
    > "$DIRECTORY_TEMP/plan-optout.txt" 2>&1 \
    || fail_test "plan refused because of the sentinel: $(cat "$DIRECTORY_TEMP/plan-optout.txt")"
grep -q 'warning: desktop integration:' "$DIRECTORY_TEMP/plan-optout.txt" \
    || fail_test "the sentinel was not reported: $(cat "$DIRECTORY_TEMP/plan-optout.txt")"
grep -q 'no_desktopintegration' "$DIRECTORY_TEMP/plan-optout.txt" \
    || fail_test "the warning does not name the file that caused it"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --no-move \
    "$DIRECTORY_TEMP/probe.AppImage" > "$DIRECTORY_TEMP/install-optout.txt" 2>&1 \
    || fail_test "install refused instead of warning: $(cat "$DIRECTORY_TEMP/install-optout.txt")"
if [ ! -f "$DIRECTORY_DATA/applications/probe.desktop" ]; then
    fail_test "install warned and then did not integrate"
fi
# The warning is shown by the install too, not only by plan: it is printed with the plan
# that install asks for before it writes anything.
grep -q 'warning: desktop integration:' "$DIRECTORY_TEMP/install-optout.txt" \
    || fail_test "install did not repeat the warning"

echo "=== the environment variable ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/probe.AppImage" \
    > /dev/null 2>&1
env DESKTOPINTEGRATION=1 \
    HOME="$DIRECTORY_XDG/home" XDG_DATA_HOME="$DIRECTORY_DATA" \
    XDG_DATA_DIRS="$DIRECTORY_XDG/share" XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
    XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
    "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/probe.AppImage" \
    > "$DIRECTORY_TEMP/plan-env.txt" 2>&1
grep -q 'DESKTOPINTEGRATION' "$DIRECTORY_TEMP/plan-env.txt" \
    || fail_test "a set DESKTOPINTEGRATION was not reported: $(cat "$DIRECTORY_TEMP/plan-env.txt")"

echo "=== a running appimaged ==="
cat > "$DIRECTORY_TEMP/appimaged.c" <<'PROGRAM'
#include <unistd.h>
int main(void) {
    sleep(600);
    return 0;
}
PROGRAM
"$FILE_COMPILER" -o "$DIRECTORY_TEMP/appimaged" "$DIRECTORY_TEMP/appimaged.c" \
    || fail_test "the appimaged stand-in could not be compiled"
"$DIRECTORY_TEMP/appimaged" &
PID_APPIMAGED=$!
sleep 1
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/probe.AppImage" \
    > "$DIRECTORY_TEMP/plan-appimaged.txt" 2>&1
grep -q 'appimaged' "$DIRECTORY_TEMP/plan-appimaged.txt" \
    || fail_test "a running appimaged was not reported: $(cat "$DIRECTORY_TEMP/plan-appimaged.txt")"
kill "$PID_APPIMAGED" 2>/dev/null || true
wait "$PID_APPIMAGED" 2>/dev/null || true
PID_APPIMAGED=

echo "=== the report the window reads carries them ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json \
    "$DIRECTORY_TEMP/probe.AppImage" > "$DIRECTORY_TEMP/explain.json" 2>&1
grep -q '"desktop_integration_optout":\[' "$DIRECTORY_TEMP/explain.json" \
    || fail_test "the JSON report has no field for the conditions"
grep -q 'no_desktopintegration' "$DIRECTORY_TEMP/explain.json" \
    || fail_test "the JSON report does not carry the condition that holds"

echo "=== removing the integration still works, sentinel or not ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier probe > /dev/null 2>&1 \
    || true
if [ -z "$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list)" ]; then
    : # already gone by identifier below, or removed above
fi
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --no-move \
    "$DIRECTORY_TEMP/probe.AppImage" > /dev/null 2>&1
identifier=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}' | head -1)
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$identifier" \
    > "$DIRECTORY_TEMP/uninstall.txt" 2>&1 \
    || fail_test "uninstall is blocked by the opt-out: $(cat "$DIRECTORY_TEMP/uninstall.txt")"
if [ -f "$DIRECTORY_DATA/applications/probe.desktop" ]; then
    fail_test "uninstall did not remove the launcher"
fi

pass_test "desktop integration opt-out"
