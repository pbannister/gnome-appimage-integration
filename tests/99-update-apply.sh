#!/bin/sh
#
# Tool-gated test: `update` downloads the offered AppImage under its own name, points the
# launcher and the record at it, and keeps the file it replaced.
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

# One payload builder: the application, its version and a marker are what differ.
make_payload() { # <directory> <version> <stem> <name> <marker>
    mkdir -p "$1/usr/share/icons/hicolor/48x48/apps"
    printf 'png-%s' "$5" > "$1/usr/share/icons/hicolor/48x48/apps/$3.png"
    printf 'diricon-%s' "$5" > "$1/.DirIcon"
    cat > "$1/org.example.$3.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=$4
Exec=$3 %U
Icon=$3
Categories=Utility;
X-AppImage-Version=$2
ENTRY
}

make_payload "$DIRECTORY_TEMP/old" 1.0.0 Probe "Probe App" old
make_payload "$DIRECTORY_TEMP/new" 2.0.0 Probe "Probe App" new-and-longer
# The other applications are different ones, so their launchers cannot be mistaken for
# competitors of the first.
make_payload "$DIRECTORY_TEMP/second" 1.0.0 Second "Second App" second
make_payload "$DIRECTORY_TEMP/second-new" 2.0.0 Second "Second App" second-new-and-longer
make_payload "$DIRECTORY_TEMP/local" 1.0.0 Local "Local App" local
make_payload "$DIRECTORY_TEMP/local-new" 2.0.0 Local "Local App" local-new-and-longer
make_payload "$DIRECTORY_TEMP/local-broken" 1.0.0 LocalBroken "Local Broken App" local-broken

DIRECTORY_WWW="$DIRECTORY_TEMP/www"
mkdir -p "$DIRECTORY_WWW"

# The .upd_info section goes into the ELF before the payload is appended; objcopy
# rewriting a file that already carries the payload would move it.
write_upd_value() { # <url>
    printf '%s' "zsync|$1" > "$DIRECTORY_TEMP/upd_value"
}
build_appimage_with_upd() { # <payload-dir> <output> <upd-url>
    write_upd_value "$3"
    objcopy --add-section ".upd_info=$DIRECTORY_TEMP/upd_value" "$FILE_ELF" \
        "$DIRECTORY_TEMP/elf-upd"
    build_synthetic_appimage "$DIRECTORY_TEMP/elf-upd" "$1" "$2" gzip
}

# The installed file, and the file the transport offers for it.
build_appimage_with_upd "$DIRECTORY_TEMP/old" "$DIRECTORY_TEMP/Probe.AppImage" \
    "$BASE_URL/Probe-latest-x86_64.AppImage.zsync"
build_appimage_with_upd "$DIRECTORY_TEMP/new" "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage" \
    "$BASE_URL/Probe-latest-x86_64.AppImage.zsync"
cat > "$DIRECTORY_WWW/Probe-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Probe-2.0.0-x86_64.AppImage
Length: 4096
ZSYNC

# A second installed file, updated with --no-backup.
build_appimage_with_upd "$DIRECTORY_TEMP/second" "$DIRECTORY_TEMP/Second.AppImage" \
    "$BASE_URL/Second-latest-x86_64.AppImage.zsync"
build_appimage_with_upd "$DIRECTORY_TEMP/second-new" \
    "$DIRECTORY_WWW/Second-latest-x86_64.AppImage" \
    "$BASE_URL/Second-latest-x86_64.AppImage.zsync"
cat > "$DIRECTORY_WWW/Second-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Second-2.0.0-x86_64.AppImage
ZSYNC

# A transport whose offered file is not an AppImage at all.
printf 'this is not an AppImage' > "$DIRECTORY_WWW/Broken-latest-x86_64.AppImage"
cat > "$DIRECTORY_WWW/Broken-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Broken-2.0.0-x86_64.AppImage
ZSYNC
build_appimage_with_upd "$DIRECTORY_TEMP/old" "$DIRECTORY_TEMP/Broken.AppImage" \
    "$BASE_URL/Broken-latest-x86_64.AppImage.zsync"

# A valid AppImage whose own .sha256_sig does not match it, which is what a corrupted or
# substituted download looks like.
printf 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff' \
    > "$DIRECTORY_TEMP/upd_sig"
write_upd_value "$BASE_URL/Sig-latest-x86_64.AppImage.zsync"
objcopy --add-section ".upd_info=$DIRECTORY_TEMP/upd_value" \
    --add-section ".sha256_sig=$DIRECTORY_TEMP/upd_sig" "$FILE_ELF" \
    "$DIRECTORY_TEMP/elf-badsig"
build_synthetic_appimage "$DIRECTORY_TEMP/elf-badsig" "$DIRECTORY_TEMP/new" \
    "$DIRECTORY_WWW/Sig-latest-x86_64.AppImage" gzip
cat > "$DIRECTORY_WWW/Sig-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Sig-2.0.0-x86_64.AppImage
ZSYNC
build_appimage_with_upd "$DIRECTORY_TEMP/old" "$DIRECTORY_TEMP/Sig.AppImage" \
    "$BASE_URL/Sig-latest-x86_64.AppImage.zsync"

# Transports whose offered file is already beside the installed one.  The .zsync file is
# served, the AppImage at the URL is not: if the tool downloaded, it would fail.
build_appimage_with_upd "$DIRECTORY_TEMP/local" "$DIRECTORY_TEMP/Local.AppImage" \
    "$BASE_URL/Local-latest-x86_64.AppImage.zsync"
build_appimage_with_upd "$DIRECTORY_TEMP/local-new" "$DIRECTORY_TEMP/Local-2.0.0-x86_64.AppImage" \
    "$BASE_URL/Local-latest-x86_64.AppImage.zsync"
cat > "$DIRECTORY_WWW/Local-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: Local-2.0.0-x86_64.AppImage
ZSYNC

build_appimage_with_upd "$DIRECTORY_TEMP/local-broken" "$DIRECTORY_TEMP/LocalBroken.AppImage" \
    "$BASE_URL/LocalBroken-latest-x86_64.AppImage.zsync"
cat > "$DIRECTORY_WWW/LocalBroken-latest-x86_64.AppImage.zsync" <<'ZSYNC'
zsync: 0.6.2
Filename: LocalBroken-2.0.0-x86_64.AppImage
ZSYNC
printf 'this is not an AppImage either' > "$DIRECTORY_TEMP/LocalBroken-2.0.0-x86_64.AppImage"

DIRECTORY_HOME="$DIRECTORY_TEMP/home"
DIRECTORY_APPLICATIONS="$DIRECTORY_HOME/.local/share/applications"
mkdir -p "$DIRECTORY_APPLICATIONS" "$DIRECTORY_TEMP/share" "$DIRECTORY_TEMP/etc" \
    "$DIRECTORY_HOME/Applications"

run_in_sandbox() {
    env HOME="$DIRECTORY_HOME" \
        XDG_DATA_HOME="$DIRECTORY_HOME/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_TEMP/share" \
        XDG_CONFIG_HOME="$DIRECTORY_HOME/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_TEMP/etc" \
        "$@"
}

version_of() { # <AppImage> -> the version the tool reads out of it
    run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$1" 2>/dev/null \
        | python3 -c 'import json,sys; print(json.load(sys.stdin)["version"])'
}
hash_of() { # <file> -> sha256
    sha256sum < "$1" | cut -d' ' -f1
}

python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DIRECTORY_WWW" \
    > "$DIRECTORY_TEMP/server.log" 2>&1 &
PID_SERVER=$!
sleep 1

FILE_MANAGED="$DIRECTORY_HOME/Applications/Probe.AppImage"
FILE_NEW="$DIRECTORY_HOME/Applications/Probe-2.0.0-x86_64.AppImage"

echo "=== two launchers for one file, and the transport's Update action ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/Probe.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes --add \
    "$DIRECTORY_TEMP/Probe.AppImage" > "$DIRECTORY_TEMP/add.txt" 2>&1
if [ "$(version_of "$FILE_MANAGED")" != "1.0.0" ]; then
    fail_test "the installed file does not report version 1.0.0"
fi
for FILE_LAUNCHER in "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    "$DIRECTORY_APPLICATIONS/org.example.Probe-2.desktop"; do
    grep -q '^Actions=AppImage-Activator;Update-AppImage;Remove-AppImage;$' "$FILE_LAUNCHER" \
        || fail_test "$FILE_LAUNCHER does not offer Update although the file carries update information"
    grep -q '^Name=Update$' "$FILE_LAUNCHER" || fail_test "$FILE_LAUNCHER has no Update item"
    grep -q ' handle --update ' "$FILE_LAUNCHER" \
        || fail_test "$FILE_LAUNCHER's Update item does not open the activator"
done

HASH_BEFORE=$(hash_of "$FILE_MANAGED")

echo "=== without --force the transport is not enough to act on ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/noforce.txt" 2>&1
grep -q 'pass --force' "$DIRECTORY_TEMP/noforce.txt" \
    || fail_test "the update did not ask for --force when the transport names no version"
if [ "$(hash_of "$FILE_MANAGED")" != "$HASH_BEFORE" ]; then
    fail_test "the update replaced the file although it was not asked to"
fi

echo "=== a dry run downloads nothing and names the new file ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --dry-run "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/dry.txt" 2>&1
grep -q 'dry run: nothing was written' "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not say it wrote nothing"
grep -q 'will be placed beside it as: Probe-2.0.0-x86_64.AppImage' "$DIRECTORY_TEMP/dry.txt" \
    || fail_test "the dry run did not name the file it would download"
if [ -f "$FILE_NEW" ]; then
    fail_test "the dry run downloaded the file"
fi

echo "=== update downloads the offered file, retargets, and keeps the old one ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes "$FILE_MANAGED" \
    > "$DIRECTORY_TEMP/update.txt" 2>&1
if [ ! -f "$FILE_NEW" ]; then
    fail_test "the offered file was not installed as its own name: $(cat "$DIRECTORY_TEMP/update.txt")"
fi
if [ -f "$FILE_MANAGED" ]; then
    fail_test "the file that was replaced is still there"
fi
grep -q '^updated: ' "$DIRECTORY_TEMP/update.txt" || fail_test "the update was not reported"
if [ "$(version_of "$FILE_NEW")" != "2.0.0" ]; then
    fail_test "the installed file is not the offered build"
fi
if [ ! -x "$FILE_NEW" ]; then
    fail_test "the downloaded file is not executable"
fi
if [ ! -f "$FILE_MANAGED.previous" ] || [ "$(hash_of "$FILE_MANAGED.previous")" != "$HASH_BEFORE" ]; then
    fail_test "the previous file was not kept as <name>.previous"
fi
# Both launchers, and both records, now run the new file.
for FILE_LAUNCHER in "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    "$DIRECTORY_APPLICATIONS/org.example.Probe-2.desktop"; do
    grep -q "^Exec=$FILE_NEW %U$" "$FILE_LAUNCHER" \
        || fail_test "$FILE_LAUNCHER was not retargeted: $(grep '^Exec=' "$FILE_LAUNCHER")"
    grep -q "^TryExec=$FILE_NEW$" "$FILE_LAUNCHER" \
        || fail_test "$FILE_LAUNCHER's TryExec was not retargeted"
done
if [ "$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | grep -c "$FILE_NEW")" != "2" ]; then
    fail_test "the records were not retargeted"
fi

echo "=== an offered file that is already here is used, not downloaded ==="
FILE_LOCAL="$DIRECTORY_HOME/Applications/Local.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/Local.AppImage" > /dev/null 2>&1
# The offered file is already sitting beside it; the URL serves no AppImage at all.
cp "$DIRECTORY_TEMP/Local-2.0.0-x86_64.AppImage" \
    "$DIRECTORY_HOME/Applications/Local-2.0.0-x86_64.AppImage"
HASH_LOCAL=$(hash_of "$FILE_LOCAL")
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes "$FILE_LOCAL" \
    > "$DIRECTORY_TEMP/local.txt" 2>&1
grep -q 'used the copy already at' "$DIRECTORY_TEMP/local.txt" \
    || fail_test "the local copy was not used: $(cat "$DIRECTORY_TEMP/local.txt")"
if [ -f "$FILE_LOCAL" ]; then
    fail_test "the file that was replaced is still there"
fi
if [ "$(version_of "$DIRECTORY_HOME/Applications/Local-2.0.0-x86_64.AppImage")" != "2.0.0" ]; then
    fail_test "the local copy is not the file that is now in use"
fi
if [ "$(hash_of "$FILE_LOCAL.previous")" != "$HASH_LOCAL" ]; then
    fail_test "the replaced file was not kept as <name>.previous"
fi
grep -q "^Exec=$DIRECTORY_HOME/Applications/Local-2.0.0-x86_64.AppImage %U$" \
    "$DIRECTORY_APPLICATIONS/org.example.Local.desktop" \
    || fail_test "the launcher of the local copy's application was not retargeted"

echo "=== a local copy that is not usable is refused, not replaced ==="
FILE_LOCAL_BROKEN="$DIRECTORY_HOME/Applications/LocalBroken.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/LocalBroken.AppImage" > /dev/null 2>&1
cp "$DIRECTORY_TEMP/LocalBroken-2.0.0-x86_64.AppImage" \
    "$DIRECTORY_HOME/Applications/LocalBroken-2.0.0-x86_64.AppImage"
HASH_LOCAL_BROKEN=$(hash_of "$FILE_LOCAL_BROKEN")
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes \
        "$FILE_LOCAL_BROKEN" > "$DIRECTORY_TEMP/local-broken.txt" 2>&1; then
    fail_test "a local copy that is not an AppImage was used"
fi
grep -q 'the file already here is not usable' "$DIRECTORY_TEMP/local-broken.txt" \
    || fail_test "the refusal does not say the local file is unusable: $(cat "$DIRECTORY_TEMP/local-broken.txt")"
grep -q 'remove ' "$DIRECTORY_TEMP/local-broken.txt" \
    || fail_test "the refusal does not say how to download instead"
if [ "$(hash_of "$FILE_LOCAL_BROKEN")" != "$HASH_LOCAL_BROKEN" ]; then
    fail_test "the refused update replaced the working file"
fi

echo "=== --no-backup removes the file instead of keeping it ==="
FILE_SECOND="$DIRECTORY_HOME/Applications/Second.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes \
    "$DIRECTORY_TEMP/Second.AppImage" > /dev/null 2>&1
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes --no-backup \
    "$FILE_SECOND" > "$DIRECTORY_TEMP/nobackup.txt" 2>&1
if [ -f "$FILE_SECOND" ]; then
    fail_test "--no-backup left the replaced file behind"
fi
if [ -f "$FILE_SECOND.previous" ]; then
    fail_test "--no-backup kept a previous file anyway"
fi
if [ ! -f "$DIRECTORY_HOME/Applications/Second-2.0.0-x86_64.AppImage" ]; then
    fail_test "--no-backup did not install the offered file"
fi

echo "=== a download that is not an AppImage is refused ==="
FILE_BROKEN="$DIRECTORY_HOME/Applications/Broken.AppImage"
cp "$DIRECTORY_TEMP/Broken.AppImage" "$FILE_BROKEN"
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes \
        "$FILE_BROKEN" > "$DIRECTORY_TEMP/broken.txt" 2>&1; then
    fail_test "a download that is not an AppImage was accepted"
fi
grep -q 'cannot be read as an AppImage' "$DIRECTORY_TEMP/broken.txt" \
    || fail_test "the refusal does not say the download is not an AppImage"
if [ ! -f "$FILE_BROKEN" ]; then
    fail_test "the refused download removed the working file"
fi
if [ -f "$FILE_BROKEN.part" ] || [ -f "$DIRECTORY_HOME/Applications/Broken-2.0.0-x86_64.AppImage" ]; then
    fail_test "the refused download was left behind"
fi

echo "=== a mismatch in the file's own signature is refused ==="
FILE_SIG="$DIRECTORY_HOME/Applications/Sig.AppImage"
cp "$DIRECTORY_TEMP/Sig.AppImage" "$FILE_SIG"
HASH_SIG=$(hash_of "$FILE_SIG")
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --force --yes \
        "$FILE_SIG" > "$DIRECTORY_TEMP/badsig.txt" 2>&1; then
    fail_test "a file whose signature does not match it was accepted"
fi
grep -q 'does not match' "$DIRECTORY_TEMP/badsig.txt" \
    || fail_test "the refusal does not say the signature does not match: $(cat "$DIRECTORY_TEMP/badsig.txt")"
if [ "$(hash_of "$FILE_SIG")" != "$HASH_SIG" ] \
    || [ -f "$DIRECTORY_HOME/Applications/Sig-2.0.0-x86_64.AppImage" ]; then
    fail_test "the refused download replaced the working file"
fi

echo "=== update --json reports the installed path ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" update --json "$FILE_NEW" \
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
