#!/bin/sh
#
# Tool-gated test: offering to run an application under firejail.
#
# firejail is not installed everywhere, so the choice is offered rather than assumed: asked
# for and absent, an install is refused rather than quietly writing a launcher that would
# fail, and a record that names a sandbox which has since gone is reported instead of being
# rewritten into something the owner did not choose.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "firejail sandbox (no mksquashfs)"
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
    pass_test "firejail sandbox (no host ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/fireprobe.png"
printf 'diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf '[Desktop Entry]\nType=Application\nName=Fire Probe\nExec=fireprobe %%F\nIcon=fireprobe\nStartupWMClass=FireProbe\n' \
    > "$DIRECTORY_PAYLOAD/fireprobe.desktop"
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/probe.AppImage" gzip

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA" "$DIRECTORY_XDG/home/.config" "$DIRECTORY_XDG/share" \
    "$DIRECTORY_XDG/etc"

# A stand-in for firejail: it records how it was called and then runs the application, so
# the test asserts the shape of the call without needing firejail's own behaviour.
mkdir -p "$DIRECTORY_TEMP/bin"
cat > "$DIRECTORY_TEMP/bin/firejail" <<'STUB'
#!/bin/sh
printf '%s\n' "$*" >> "$STUB_RECORD"
exec "$@"
STUB
chmod 755 "$DIRECTORY_TEMP/bin/firejail"

run_with_sandbox_present() {
    env -u DISPLAY -u WAYLAND_DISPLAY -u DESKTOPINTEGRATION \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        PATH="$DIRECTORY_TEMP/bin:$PATH" \
        "$@"
}

run_without_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY -u DESKTOPINTEGRATION \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

FILE_TOOL="$DIRECTORY_BUILD/appimage-integrate"
FILE_LAUNCHER="$DIRECTORY_DATA/applications/fireprobe.desktop"

echo "=== asked for and absent, it is refused rather than dropped ==="
if run_without_sandbox "$FILE_TOOL" install --yes --no-move --sandbox firejail \
    "$DIRECTORY_TEMP/probe.AppImage" > "$DIRECTORY_TEMP/absent.txt" 2>&1; then
    fail_test "an install under a sandbox that is not installed was allowed"
fi
grep -q 'firejail is not installed' "$DIRECTORY_TEMP/absent.txt" \
    || fail_test "the refusal does not say what is missing: $(cat "$DIRECTORY_TEMP/absent.txt")"
if [ -e "$FILE_LAUNCHER" ]; then
    fail_test "a launcher was written for an install that was refused"
fi

echo "=== an unknown sandbox is refused by the option itself ==="
if run_with_sandbox_present "$FILE_TOOL" install --yes --no-move --sandbox bubblewrap \
    "$DIRECTORY_TEMP/probe.AppImage" > "$DIRECTORY_TEMP/unknown.txt" 2>&1; then
    fail_test "an unknown sandbox name was accepted"
fi
grep -q -- '--sandbox takes none or firejail' "$DIRECTORY_TEMP/unknown.txt" \
    || fail_test "the unknown name was not explained: $(cat "$DIRECTORY_TEMP/unknown.txt")"

echo "=== with firejail present, the launcher runs through it ==="
STUB_RECORD="$DIRECTORY_TEMP/firejail-calls.txt" run_with_sandbox_present \
    "$FILE_TOOL" install --yes --no-move --sandbox firejail "$DIRECTORY_TEMP/probe.AppImage" \
    > "$DIRECTORY_TEMP/install.txt" 2>&1 \
    || fail_test "the install with firejail present failed: $(cat "$DIRECTORY_TEMP/install.txt")"
grep -q '^sandbox: firejail$' "$DIRECTORY_TEMP/install.txt" \
    || fail_test "the install does not report the sandbox it wrote"
grep -qx 'Exec=firejail --appimage .*probe.AppImage %F' "$FILE_LAUNCHER" \
    || fail_test "the launcher's Exec is not firejail's AppImage form: $(grep '^Exec=' "$FILE_LAUNCHER")"
grep -q '^sandbox=firejail$' "$DIRECTORY_DATA"/gnome-appimage-integration/*.manifest \
    || fail_test "the record does not say the application runs under firejail"
# firejail's own order: its options, --appimage, then the file and its arguments.
grep -q '^Exec=firejail --appimage ' "$FILE_LAUNCHER" \
    || fail_test "firejail is not invoked in its documented order"

echo "=== a rewrite keeps the choice ==="
run_with_sandbox_present "$FILE_TOOL" refresh --yes > /dev/null 2>&1 || true
grep -qx 'Exec=firejail --appimage .*probe.AppImage %F' "$FILE_LAUNCHER" \
    || fail_test "a rewrite dropped the sandbox: $(grep '^Exec=' "$FILE_LAUNCHER")"

echo "=== and when firejail goes, the record is honoured rather than rewritten ==="
run_without_sandbox "$FILE_TOOL" audit > "$DIRECTORY_TEMP/audit.txt" 2>&1 || true
grep -q 'firejail' "$DIRECTORY_TEMP/audit.txt" \
    || fail_test "audit does not report a launcher whose sandbox is missing: $(cat "$DIRECTORY_TEMP/audit.txt")"
if run_without_sandbox "$FILE_TOOL" refresh --yes > "$DIRECTORY_TEMP/refresh.txt" 2>&1; then
    fail_test "refresh rewrote a launcher whose sandbox is not installed"
fi
grep -qx 'Exec=firejail --appimage .*probe.AppImage %F' "$FILE_LAUNCHER" \
    || fail_test "the launcher was changed even though the rewrite was refused"

echo "=== and asking for none again is how it is undone ==="
run_with_sandbox_present "$FILE_TOOL" install --yes --no-move --sandbox none \
    "$DIRECTORY_TEMP/probe.AppImage" > /dev/null 2>&1 \
    || fail_test "re-integrating without a sandbox failed"
grep -qx 'Exec=.*probe.AppImage %F' "$FILE_LAUNCHER" \
    || fail_test "the launcher still runs through firejail: $(grep '^Exec=' "$FILE_LAUNCHER")"
if grep -q '^sandbox=' "$DIRECTORY_DATA"/gnome-appimage-integration/*.manifest; then
    fail_test "the record still names a sandbox that was turned off"
fi

pass_test "firejail sandbox"
