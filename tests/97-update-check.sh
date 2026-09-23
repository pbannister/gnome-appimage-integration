#!/bin/sh
#
# Tool-gated test: the update-information value, what it resolves to, and `update --check`.
#
# Prerequisites: mksquashfs, objcopy and od for the synthetic AppImage, and python3 for
# a local HTTP server.  The server is on 127.0.0.1, so no outside network is used; the
# GitHub transport is exercised offline, through the same code path that reads it.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1 \
    || ! command -v objcopy >/dev/null 2>&1; then
    echo "SKIP: mksquashfs, od or objcopy is not available (tool-gated)"
    pass_test "update check (no mksquashfs or objcopy)"
    exit 0
fi

FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done
if [ -z "$FILE_ELF" ] || [ "$(od -An -tu1 -j 4 -N 1 "$FILE_ELF" | tr -d ' \n')" != "2" ]; then
    echo "SKIP: no 64-bit host ELF executable found"
    pass_test "update check (no host ELF)"
    exit 0
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: python3 is not available, so no local HTTP server"
    pass_test "update check (no python3)"
    exit 0
fi

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

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
cat > "$DIRECTORY_PAYLOAD/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
Exec=probe %U
Icon=probe
Categories=Utility;
X-AppImage-Version=1.0.0
ENTRY

build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Probe.AppImage" gzip

# The section goes into the ELF before the payload is appended, the way tests/92 does
# it: objcopy rewriting a file that already carries the payload would move it.
build_with_upd_info() { # <value-file> <output>
    objcopy --add-section ".upd_info=$1" "$FILE_ELF" "$DIRECTORY_TEMP/elf-upd"
    build_synthetic_appimage "$DIRECTORY_TEMP/elf-upd" "$DIRECTORY_PAYLOAD" "$2" gzip
}

printf 'zsync|http://127.0.0.1:8080/Probe-latest-x86_64.AppImage.zsync' \
    > "$DIRECTORY_TEMP/upd_info"
build_with_upd_info "$DIRECTORY_TEMP/upd_info" "$DIRECTORY_TEMP/ProbeUpdate.AppImage"

echo "=== the value is resolved without asking anyone ==="
"$DIRECTORY_BUILD/appimage-inspect" --update-url "$DIRECTORY_TEMP/ProbeUpdate.AppImage" \
    > "$DIRECTORY_TEMP/resolved.txt" 2>&1
grep -q '^update-transport: zsync$' "$DIRECTORY_TEMP/resolved.txt" \
    || fail_test "the zsync transport was not named"
grep -q '^update-request-url: http://127.0.0.1:8080/Probe-latest-x86_64.AppImage.zsync$' \
    "$DIRECTORY_TEMP/resolved.txt" || fail_test "the request URL was not resolved"
grep -q '^update-image-pattern: http://127.0.0.1:8080/Probe-latest-x86_64.AppImage' \
    "$DIRECTORY_TEMP/resolved.txt" || fail_test "the image pattern was not derived"
grep -q '^update-usable: yes$' "$DIRECTORY_TEMP/resolved.txt" \
    || fail_test "a complete value was not called usable"

echo "=== the GitHub transport resolves to its API URL ==="
cat > "$DIRECTORY_PAYLOAD/org.example.Gh.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe GitHub
Exec=probe %U
Icon=probe
Categories=Utility;
X-AppImage-Version=1.1.1
ENTRY
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Gh.AppImage" gzip
printf 'gh-releases-zsync|FreeCAD|FreeCAD|latest|FreeCAD*x86_64*.AppImage.zsync' \
    > "$DIRECTORY_TEMP/upd_info_gh"
build_with_upd_info "$DIRECTORY_TEMP/upd_info_gh" "$DIRECTORY_TEMP/GhUpdate.AppImage"
"$DIRECTORY_BUILD/appimage-inspect" --update-url "$DIRECTORY_TEMP/GhUpdate.AppImage" \
    > "$DIRECTORY_TEMP/resolved-gh.txt" 2>&1
grep -q '^update-request-url: https://api.github.com/repos/FreeCAD/FreeCAD/releases/latest$' \
    "$DIRECTORY_TEMP/resolved-gh.txt" || fail_test "latest did not resolve to the API URL"
grep -q '^update-image-pattern: FreeCAD\*x86_64\*.AppImage' "$DIRECTORY_TEMP/resolved-gh.txt" \
    || fail_test "the GitHub image pattern was not derived"

echo "=== explain carries it for the graphical activator ==="
"$DIRECTORY_BUILD/appimage-integrate" explain --json "$DIRECTORY_TEMP/GhUpdate.AppImage" \
    > "$DIRECTORY_TEMP/explain.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/explain.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["update_usable"] is True, data
assert "FreeCAD/FreeCAD" in data["update_description"], data
assert data["update_problem"] == "", data
print("explain json update fields ok")
PYTHON

echo "=== a value that is not a transport is reported as unusable ==="
printf 'guess' > "$DIRECTORY_TEMP/upd_info_guess"
build_with_upd_info "$DIRECTORY_TEMP/upd_info_guess" "$DIRECTORY_TEMP/Guess.AppImage"
"$DIRECTORY_BUILD/appimage-inspect" --update-url "$DIRECTORY_TEMP/Guess.AppImage" \
    > "$DIRECTORY_TEMP/guess.txt" 2>&1
grep -q '^update-usable: no$' "$DIRECTORY_TEMP/guess.txt" \
    || fail_test "an unknown transport was called usable"
grep -q 'not a transport the AppImage specification defines' "$DIRECTORY_TEMP/guess.txt" \
    || fail_test "the unknown transport was not explained"
if "$DIRECTORY_BUILD/appimage-integrate" update --check "$DIRECTORY_TEMP/Guess.AppImage" \
        > "$DIRECTORY_TEMP/guess-check.txt" 2>&1; then
    fail_test "checking an unusable value succeeded"
fi
grep -q 'not a transport the AppImage specification defines' "$DIRECTORY_TEMP/guess-check.txt" \
    || fail_test "the check did not report the unusable value"

echo "=== the write path refuses a value it cannot check ==="
# Without --check, update is the command that replaces the file; it must still ask the
# transport first, and refuse when the value is not one it can follow.
if "$DIRECTORY_BUILD/appimage-integrate" update --yes "$DIRECTORY_TEMP/Guess.AppImage" \
        > "$DIRECTORY_TEMP/bare.txt" 2>&1; then
    fail_test "update acted on an update information value it cannot check"
fi
grep -q 'cannot check' "$DIRECTORY_TEMP/bare.txt" \
    || fail_test "the refusal does not say why nothing was downloaded"

echo "=== a local zsync server answers the check ==="
DIRECTORY_WWW="$DIRECTORY_TEMP/www"
mkdir -p "$DIRECTORY_WWW"
cat > "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Probe-2.0.0-x86_64.AppImage
MTime: Fri, 01 Jan 2027 00:00:00 +0000
Blocksize: 2048
Length: 123456
ZSYNC
cat > "$DIRECTORY_WWW/Other-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Probe.AppImage
Length: 111
ZSYNC
# The AppImage's own name is ProbeUpdate.AppImage, so the first server names a different
# file and the second names the file that is installed.
python3 -m http.server 8080 --bind 127.0.0.1 --directory "$DIRECTORY_WWW" \
    > "$DIRECTORY_TEMP/server.log" 2>&1 &
PID_SERVER=$!
sleep 1

"$DIRECTORY_BUILD/appimage-integrate" update --check "$DIRECTORY_TEMP/ProbeUpdate.AppImage" \
    > "$DIRECTORY_TEMP/check.txt" 2>&1 || true
grep -q 'a different file is offered: Probe-2.0.0-x86_64.AppImage' "$DIRECTORY_TEMP/check.txt" \
    || fail_test "the check did not report the offered file: $(cat "$DIRECTORY_TEMP/check.txt")"
grep -q 'the transport names no version' "$DIRECTORY_TEMP/check.txt" \
    || fail_test "the check claimed more than the transport says"

# The zsync file names Probe.AppImage, and so does this copy, so the check must say the
# transport offers the file that is installed.
mkdir -p "$DIRECTORY_TEMP/same"
printf 'zsync|http://127.0.0.1:8080/Other-latest-x86_64.AppImage.zsync' \
    > "$DIRECTORY_TEMP/upd_info_same"
build_with_upd_info "$DIRECTORY_TEMP/upd_info_same" "$DIRECTORY_TEMP/same/Probe.AppImage"
"$DIRECTORY_BUILD/appimage-integrate" update --check "$DIRECTORY_TEMP/same/Probe.AppImage" \
    > "$DIRECTORY_TEMP/same.txt" 2>&1
grep -q 'up to date: the transport offers the file that is installed' "$DIRECTORY_TEMP/same.txt" \
    || fail_test "the check did not recognise the installed file name"

echo "=== update --check --json reports the fields ==="
"$DIRECTORY_BUILD/appimage-integrate" update --check --json \
    "$DIRECTORY_TEMP/ProbeUpdate.AppImage" > "$DIRECTORY_TEMP/check.json" 2>&1
python3 - "$DIRECTORY_TEMP/check.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert len(data["checks"]) == 1, data
check = data["checks"][0]
assert check["appimage_asset"] == "Probe-2.0.0-x86_64.AppImage", check
assert check["appimage_asset_size"] == 123456, check
assert check["installed_version"] == "1.0.0", check
assert check["problem"] == "", check
print("update check json ok")
PYTHON

echo "=== the launcher gets a Check for updates action ==="
DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_APPLICATIONS="$DIRECTORY_XDG/home/.local/share/applications"
mkdir -p "$DIRECTORY_APPLICATIONS" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"
env HOME="$DIRECTORY_XDG/home" \
    XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
    XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
    XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
    XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
    "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/ProbeUpdate.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
grep -q '^Actions=AppImage-Activator;Update-AppImage;Remove-AppImage;$' \
    "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the launcher does not list the update action"
grep -q '^\[Desktop Action Update-AppImage\]$' \
    "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the launcher has no update action group"
grep -q 'update --check --notify' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the update action does not run the check"

echo "=== audit reports the update information of what it manages ==="
env HOME="$DIRECTORY_XDG/home" \
    XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
    XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
    XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
    XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
    "$DIRECTORY_BUILD/appimage-integrate" audit > "$DIRECTORY_TEMP/audit.txt" 2>&1 || true
if grep -q 'has no update information' "$DIRECTORY_TEMP/audit.txt"; then
    fail_test "audit reported a missing value for an AppImage that carries one"
fi
grep -q 'ProbeUpdate.AppImage' "$DIRECTORY_TEMP/audit.txt" && \
    fail_test "audit complained about a usable update information value"

pass_test "update check"
