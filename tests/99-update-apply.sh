#!/bin/sh
#
# Tool-gated test: `update` replaces an installed AppImage with the file the transport
# offers, after verifying the download, and re-renders the launcher.
#
# Prerequisites: mksquashfs, objcopy and od for the synthetic AppImages, and python3 for
# a local HTTP server on 127.0.0.1.  No outside network is used.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1 \
    || ! command -v objcopy >/dev/null 2>&1; then
    echo "SKIP: mksquashfs, od or objcopy is not available (tool-gated)"
    pass_test "update apply (no mksquashfs or objcopy)"
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
    pass_test "update apply (no host ELF)"
    exit 0
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "SKIP: python3 is not available, so no local HTTP server"
    pass_test "update apply (no python3)"
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

PORT=8081
BASE_URL="http://127.0.0.1:$PORT"

# The AppImage being updated: version 1.0.0, and it embeds the transport to use.
DIRECTORY_OLD="$DIRECTORY_TEMP/payload-old"
mkdir -p "$DIRECTORY_OLD/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png-old' > "$DIRECTORY_OLD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon-old' > "$DIRECTORY_OLD/.DirIcon"
cat > "$DIRECTORY_OLD/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
Exec=probe %U
Icon=probe
Categories=Utility;
X-AppImage-Version=1.0.0
ENTRY

# The AppImage the transport offers: version 2.0.0.
DIRECTORY_NEW="$DIRECTORY_TEMP/payload-new"
mkdir -p "$DIRECTORY_NEW/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png-new-and-longer' > "$DIRECTORY_NEW/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon-new' > "$DIRECTORY_NEW/.DirIcon"
cat > "$DIRECTORY_NEW/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
Exec=probe %U
Icon=probe
Categories=Utility;
X-AppImage-Version=2.0.0
ENTRY

DIRECTORY_WWW="$DIRECTORY_TEMP/www"
mkdir -p "$DIRECTORY_WWW"
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_OLD" "$DIRECTORY_TEMP/Old.AppImage" gzip

# The .upd_info section goes into the ELF before the payload is appended.
build_with_upd_info() { # <value> <output> <payload-dir>
    printf '%s' "$1" > "$DIRECTORY_TEMP/upd_value"
    objcopy --add-section ".upd_info=$DIRECTORY_TEMP/upd_value" "$FILE_ELF" \
        "$DIRECTORY_TEMP/elf-upd"
    build_synthetic_appimage "$DIRECTORY_TEMP/elf-upd" "$3" "$2" gzip
}
UPD_VALUE="zsync|$BASE_URL/Probe-latest-x86_64.AppImage.zsync"
build_with_upd_info "$UPD_VALUE" "$DIRECTORY_TEMP/Probe.AppImage" "$DIRECTORY_OLD"
# The offered build carries the same transport, so the updated file can be updated again.
build_with_upd_info "$UPD_VALUE" "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage" "$DIRECTORY_NEW"

cat > "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Probe-2.0.0-x86_64.AppImage
Length: 4096
ZSYNC

# A second transport, whose offered file is not an AppImage at all.
printf 'this is not an AppImage' > "$DIRECTORY_WWW/Broken-latest-x86_64.AppImage"
cat > "$DIRECTORY_WWW/Broken-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Broken-2.0.0-x86_64.AppImage
ZSYNC

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_HOME="$DIRECTORY_XDG/home"
FILE_MANAGED="$DIRECTORY_HOME/Applications/Probe.AppImage"
mkdir -p "$DIRECTORY_HOME/.local/share/applications" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc" \
    "$DIRECTORY_HOME/Applications"

run_in_sandbox() {
    env HOME="$DIRECTORY_HOME" \
        XDG_DATA_HOME="$DIRECTORY_HOME/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_HOME/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIRECTORY_WWW" \
    > "$DIRECTORY_TEMP/server.log" 2>&1 &
PID_SERVER=$!
sleep 1

echo "=== the AppImage is integrated first ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/Probe.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
HASH_BEFORE=$(sha256sum < "$FILE_MANAGED" | cut -d" " -f1)
version_of() { # <AppImage> -> its version, from the tool that extracts it
    run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$1" 2>/dev/null \
        | python3 -c 'import json,sys; print(json.load(sys.stdin)["version"])'
}
if [ "$(version_of "$FILE_MANAGED")" != "1.0.0" ]; then
    fail_test "the installed file does not report version 1.0.0"
fi

echo "=== without --force the transport is not enough to act on ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/noforce.txt" 2>&1
grep -q 'pass --force' "$DIRECTORY_TEMP/noforce.txt" \
    || fail_test "the update did not ask for --force when the transport names no version"
if [ "$(sha256sum < "$FILE_MANAGED" | cut -d" " -f1)" != "$HASH_BEFORE" ]; then
    fail_test "the update replaced the file although it was not asked to"
fi

echo "=== a dry run downloads nothing ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --dry-run "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/dry.txt" 2>&1
grep -q 'dry run: nothing was written' "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not say it wrote nothing"
grep -q 'download: ' "$DIRECTORY_TEMP/dry.txt" || fail_test "the dry run named no download"
if [ "$(sha256sum < "$FILE_MANAGED" | cut -d" " -f1)" != "$HASH_BEFORE" ]; then
    fail_test "the dry run replaced the file"
fi

echo "=== update --force replaces the file and re-renders the launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes --backup \
    "$FILE_MANAGED" > "$DIRECTORY_TEMP/update.txt" 2>&1
HASH_AFTER=$(sha256sum < "$FILE_MANAGED" | cut -d" " -f1)
if [ "$HASH_AFTER" = "$HASH_BEFORE" ]; then
    fail_test "the file was not replaced: $(cat "$DIRECTORY_TEMP/update.txt")"
fi
grep -q '^updated: ' "$DIRECTORY_TEMP/update.txt" || fail_test "the update was not reported"
grep -q 'launcher re-rendered:' "$DIRECTORY_TEMP/update.txt" \
    || fail_test "the launcher was not re-rendered"
if [ ! -f "$FILE_MANAGED.previous" ]; then
    fail_test "--backup did not keep the previous file"
fi
if [ "$(sha256sum < "$FILE_MANAGED.previous" | cut -d" " -f1)" != "$HASH_BEFORE" ]; then
    fail_test "the kept file is not the one that was replaced"
fi
if [ ! -x "$FILE_MANAGED" ]; then
    fail_test "the replaced file is not executable"
fi
FILE_VERSION=$(version_of "$FILE_MANAGED")
if [ "$FILE_VERSION" != "2.0.0" ]; then
    fail_test "the replaced file is version $FILE_VERSION instead of 2.0.0"
fi
grep -q "^Exec=$FILE_MANAGED %U$" \
    "$DIRECTORY_HOME/.local/share/applications/org.example.Probe.desktop" \
    || fail_test "the launcher no longer runs the managed file"

echo "=== a download that is not an AppImage is refused ==="
build_with_upd_info "zsync|$BASE_URL/Broken-latest-x86_64.AppImage.zsync" \
    "$DIRECTORY_TEMP/Broken.AppImage" "$DIRECTORY_OLD"
cp "$DIRECTORY_TEMP/Broken.AppImage" "$DIRECTORY_HOME/Applications/Broken.AppImage"
HASH_BROKEN=$(sha256sum < "$DIRECTORY_HOME/Applications/Broken.AppImage" | cut -d" " -f1)
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes \
        "$DIRECTORY_HOME/Applications/Broken.AppImage" > "$DIRECTORY_TEMP/broken.txt" 2>&1; then
    fail_test "a download that is not an AppImage was accepted"
fi
grep -q 'cannot be read as an AppImage' "$DIRECTORY_TEMP/broken.txt" \
    || fail_test "the refusal does not say the download is not an AppImage"
if [ "$(sha256sum < "$DIRECTORY_HOME/Applications/Broken.AppImage" | cut -d" " -f1)" != "$HASH_BROKEN" ]; then
    fail_test "the refused download replaced the file"
fi
if [ -f "$DIRECTORY_HOME/Applications/Broken.AppImage.part" ]; then
    fail_test "the refused download was left behind"
fi

echo "=== a mismatch in the file's own signature is refused ==="
# The offered file is a valid AppImage whose .sha256_sig holds a digest that is not
# the file's, which is what a corrupted or substituted download looks like.
printf 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff' \
    > "$DIRECTORY_TEMP/upd_sig"
objcopy --add-section ".upd_info=$DIRECTORY_TEMP/upd_value" \
    --add-section ".sha256_sig=$DIRECTORY_TEMP/upd_sig" "$FILE_ELF" \
    "$DIRECTORY_TEMP/elf-badsig"
build_synthetic_appimage "$DIRECTORY_TEMP/elf-badsig" "$DIRECTORY_NEW" \
    "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage" gzip
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes \
        "$FILE_MANAGED" > "$DIRECTORY_TEMP/badsig.txt" 2>&1; then
    fail_test "a file whose signature does not match it was accepted"
fi
grep -q 'does not match' "$DIRECTORY_TEMP/badsig.txt" \
    || fail_test "the refusal does not say the signature does not match: $(cat "$DIRECTORY_TEMP/badsig.txt")"
if [ "$(sha256sum < "$FILE_MANAGED" | cut -d" " -f1)" != "$HASH_AFTER" ]; then
    fail_test "the refused download replaced the working file"
fi

echo "=== update --json reports what was written ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --json "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/update.json" 2>&1 || true
python3 - "$DIRECTORY_TEMP/update.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert "updates" in data and "written" in data, data
assert data["written"] == 0, data
print("update json ok")
PYTHON

pass_test "update apply"
