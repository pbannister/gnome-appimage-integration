#!/bin/sh
#
# Tool-gated test: the `.sha256_sig` section is classified and checked.
# The digest covers the whole file with the section zeroed, as the AppImage
# specification says, so the test writes a real ELF section with objcopy and then
# puts the digest of the assembled file into it.
# Prerequisites: mksquashfs, od, dd, sha256sum, objcopy, and a 64-bit host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

for tool_name in mksquashfs od dd sha256sum objcopy; do
    if ! command -v "$tool_name" >/dev/null 2>&1; then
        echo "SKIP: $tool_name is not available (tool-gated)"
        exit 0
    fi
done

FILE_ELF=
for candidate_elf in /bin/true /bin/false /usr/bin/env; do
    if [ -f "$candidate_elf" ]; then
        FILE_ELF=$candidate_elf
        break
    fi
done
if [ -z "$FILE_ELF" ] || [ "$(od -An -tu1 -j 4 -N 1 "$FILE_ELF" | tr -d ' \n')" != "2" ]; then
    echo "SKIP: no 64-bit host ELF executable found"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

SIGNATURE_SECTION_SIZE=256
DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
cat > "$DIRECTORY_PAYLOAD/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
X-AppImage-Version=9.9.9
Exec=probe %U
Icon=probe
StartupWMClass=ProbeApp
ENTRY

# An ELF that already carries the section, filled with zeros: that is what tooling
# signs, so the digest it records is the digest of the file with the section zeroed.
dd if=/dev/zero of="$DIRECTORY_TEMP/zeros.bin" bs=1 count=$SIGNATURE_SECTION_SIZE 2>/dev/null
objcopy --add-section ".sha256_sig=$DIRECTORY_TEMP/zeros.bin" \
    "$FILE_ELF" "$DIRECTORY_TEMP/elf-signed" 2>/dev/null

# The payload is built once and appended as-is, so the digest can be computed over
# exactly the bytes that end up in the file.
mksquashfs "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/payload.squashfs" -comp gzip -noappend -quiet \
    >/dev/null

build_signed_appimage() { # <output> <digest-text-or-empty>
    file_output=$1
    digest_text=$2
    build_synthetic_appimage "$DIRECTORY_TEMP/elf-signed" "$DIRECTORY_PAYLOAD" "$file_output" \
        gzip "$DIRECTORY_TEMP/payload.squashfs"
    if [ -n "$digest_text" ]; then
        # Fill the section in place: the digest was computed with it zeroed, so
        # writing it here leaves the digest valid.
        dd if=/dev/zero of="$file_output.digest" bs=1 count=$SIGNATURE_SECTION_SIZE 2>/dev/null
        printf '%s' "$digest_text" | dd of="$file_output.digest" bs=1 conv=notrunc 2>/dev/null
        objcopy --update-section ".sha256_sig=$file_output.digest" "$DIRECTORY_TEMP/elf-signed" \
            "$DIRECTORY_TEMP/elf-signed-updated" 2>/dev/null
        mv "$DIRECTORY_TEMP/elf-signed-updated" "$DIRECTORY_TEMP/elf-signed"
        # Rebuild with the same payload: only the section bytes differ from the file
        # the digest was computed over.
        build_synthetic_appimage "$DIRECTORY_TEMP/elf-signed" "$DIRECTORY_PAYLOAD" "$file_output" \
            gzip "$DIRECTORY_TEMP/payload.squashfs"
        rm -f "$file_output.digest"
    fi
}

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
mkdir -p "$DIRECTORY_XDG/home/.local/share/applications" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"
run_in_sandbox() {
    env HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

relation_of_signature() { # <AppImage> -> the "signature" field of explain --json
    run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$1" 2>/dev/null \
        | python3 -c "import json,sys; data=json.load(sys.stdin); print(data['signature_mismatch'], '|', data['signature'])"
}

echo "=== a zero-padded section has nothing to check ==="
build_signed_appimage "$DIRECTORY_TEMP/Zero.AppImage" ""
FILE_LINE=$(relation_of_signature "$DIRECTORY_TEMP/Zero.AppImage")
if [ "$FILE_LINE" != "False | present (empty padding)" ]; then
    fail_test "an empty section reported: $FILE_LINE"
fi

echo "=== the digest of the file with the section zeroed verifies ==="
build_signed_appimage "$DIRECTORY_TEMP/Good.AppImage" "$(sha256sum "$DIRECTORY_TEMP/Zero.AppImage" | cut -d' ' -f1)"
FILE_LINE=$(relation_of_signature "$DIRECTORY_TEMP/Good.AppImage")
if [ "$FILE_LINE" != "False | present, payload digest verified" ]; then
    fail_test "a correct digest reported: $FILE_LINE"
fi
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/Good.AppImage" \
    > "$DIRECTORY_TEMP/good.txt" 2>&1
grep -q '^signature: present, payload digest verified$' "$DIRECTORY_TEMP/good.txt" \
    || fail_test "the plan does not report the verified digest"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain "$DIRECTORY_TEMP/Good.AppImage" \
    > "$DIRECTORY_TEMP/good-explain.txt" 2>&1
grep -q '^signature:    present, payload digest verified$' "$DIRECTORY_TEMP/good-explain.txt" \
    || fail_test "the report does not show the verified digest"
# The container reader reports the same result on its own.  This runs before the
# install, which moves the AppImage into the managed directory.
"$DIRECTORY_BUILD/appimage-inspect" "$DIRECTORY_TEMP/Good.AppImage" \
    > "$DIRECTORY_TEMP/inspect.txt" 2>&1
grep -q '^signature: present, payload digest verified size=' "$DIRECTORY_TEMP/inspect.txt" \
    || fail_test "appimage-inspect does not report the verified digest"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$DIRECTORY_TEMP/Good.AppImage" \
    > /dev/null 2>&1 \
    || fail_test "a verified AppImage was not integrated"

echo "=== a digest that does not match the payload is a mismatch ==="
DIRECTORY_OTHER="$DIRECTORY_TEMP/other-payload"
mkdir -p "$DIRECTORY_OTHER/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_OTHER/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'diricon-other' > "$DIRECTORY_OTHER/.DirIcon"
cp "$DIRECTORY_PAYLOAD/org.example.Probe.desktop" "$DIRECTORY_OTHER/"
mksquashfs "$DIRECTORY_OTHER" "$DIRECTORY_TEMP/other.squashfs" -comp gzip -noappend -quiet >/dev/null
build_signed_appimage "$DIRECTORY_TEMP/Bad.AppImage" \
    "$(sha256sum "$DIRECTORY_TEMP/Zero.AppImage" | cut -d' ' -f1)"
# Replace the payload with different bytes, leaving the recorded digest alone.
build_synthetic_appimage "$DIRECTORY_TEMP/elf-signed" "$DIRECTORY_OTHER" \
    "$DIRECTORY_TEMP/Bad.AppImage" gzip "$DIRECTORY_TEMP/other.squashfs"
FILE_LINE=$(relation_of_signature "$DIRECTORY_TEMP/Bad.AppImage")
if [ "$FILE_LINE" != "True | present, payload digest MISMATCH" ]; then
    fail_test "an altered payload reported: $FILE_LINE"
fi
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$DIRECTORY_TEMP/Bad.AppImage" \
    > "$DIRECTORY_TEMP/bad-install.txt" 2>&1; then
    fail_test "install accepted a payload that does not match its digest"
fi
grep -q -- '--ignore-signature' "$DIRECTORY_TEMP/bad-install.txt" \
    || fail_test "the refusal does not name the way to override it"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --ignore-signature \
    "$DIRECTORY_TEMP/Bad.AppImage" > "$DIRECTORY_TEMP/bad-forced.txt" 2>&1 \
    || fail_test "the override did not integrate the file"
grep -q 'ignored on request' "$DIRECTORY_TEMP/bad-forced.txt" \
    || fail_test "the override is not recorded in the plan"
if [ ! -f "$DIRECTORY_XDG/home/.local/share/applications/org.example.Probe.desktop" ]; then
    fail_test "the overridden integration wrote no launcher"
fi

echo "=== the update information section is reported, and padding or binary is not ==="
# The section is fixed size, so a real value is followed by NUL padding.
printf 'gh-releases-zsync|user|repo|latest|App-*.AppImage.zsync' > "$DIRECTORY_TEMP/upd.bin"
dd if=/dev/zero bs=1 count=64 >> "$DIRECTORY_TEMP/upd.bin" 2>/dev/null
objcopy --add-section ".upd_info=$DIRECTORY_TEMP/upd.bin" "$DIRECTORY_TEMP/elf-signed" \
    "$DIRECTORY_TEMP/elf-upd" 2>/dev/null
build_synthetic_appimage "$DIRECTORY_TEMP/elf-upd" "$DIRECTORY_PAYLOAD" \
    "$DIRECTORY_TEMP/Upd.AppImage" gzip "$DIRECTORY_TEMP/payload.squashfs"
FILE_UPDATE=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json \
    "$DIRECTORY_TEMP/Upd.AppImage" 2>/dev/null \
    | python3 -c "import json,sys; print(json.load(sys.stdin)['update_information'])")
if [ "$FILE_UPDATE" != "gh-releases-zsync|user|repo|latest|App-*.AppImage.zsync" ]; then
    fail_test "a NUL-padded update information section reported: [$FILE_UPDATE]"
fi

# Sections holding binary are ignored, as the specification asks.
printf '\001\002\003\004\005\006\007\010' > "$DIRECTORY_TEMP/binary-upd.bin"
objcopy --add-section ".upd_info=$DIRECTORY_TEMP/binary-upd.bin" "$DIRECTORY_TEMP/elf-signed" \
    "$DIRECTORY_TEMP/elf-bin-upd" 2>/dev/null
build_synthetic_appimage "$DIRECTORY_TEMP/elf-bin-upd" "$DIRECTORY_PAYLOAD" \
    "$DIRECTORY_TEMP/BinaryUpd.AppImage" gzip "$DIRECTORY_TEMP/payload.squashfs"
FILE_UPDATE=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json \
    "$DIRECTORY_TEMP/BinaryUpd.AppImage" 2>/dev/null \
    | python3 -c "import json,sys; print(json.load(sys.stdin)['update_information'])")
if [ -n "$FILE_UPDATE" ]; then
    fail_test "a binary update information section was reported as [$FILE_UPDATE]"
fi

pass_test "appimage signature"
