#!/bin/sh
#
# Tool-gated test: how an AppImage mounts its payload, and whether this machine can.
#
# A type-2 AppImage mounts its payload.  The runtime built before the static ones loads
# libfuse.so.2 by name, so `readelf -d` shows no dependency on it -- the only evidence in
# the file is the string the runtime hands to dlopen, which is why the reader looks for it
# in the runtime's bytes.  A machine without libfuse2, or without /dev/fuse, or without a
# fusermount binary, cannot start such a file, and the plan has to say so.
#
# Prerequisites: mksquashfs, od, objcopy, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

for tool_needed in mksquashfs od objcopy; do
    if ! command -v "$tool_needed" >/dev/null 2>&1; then
        echo "SKIP: $tool_needed is not available (tool-gated)"
        pass_test "appimage fuse mount (no $tool_needed)"
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
    pass_test "appimage fuse mount (no host ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

echo "=== the verdict itself, every combination ==="
"$DIRECTORY_BUILD/appimage-mount-test" || fail_test "the mount verdict unit test failed"

# Two runtimes that differ only in the string the runtime would hand to dlopen.  The ELF is
# given an extra section rather than patched, so the payload offset stays what the builder
# computes from the file it is handed.
printf 'libfuse.so.2' > "$DIRECTORY_TEMP/needs-fuse2.bin"
printf 'a runtime that links FUSE itself' > "$DIRECTORY_TEMP/needs-nothing.bin"
objcopy --add-section .fuse="$DIRECTORY_TEMP/needs-fuse2.bin" "$FILE_ELF" \
    "$DIRECTORY_TEMP/elf-fuse2" || fail_test "objcopy could not add the section"
objcopy --add-section .fuse="$DIRECTORY_TEMP/needs-nothing.bin" "$FILE_ELF" \
    "$DIRECTORY_TEMP/elf-static" || fail_test "objcopy could not add the section"

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/mountprobe.png"
printf 'diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
printf '[Desktop Entry]\nType=Application\nName=Mount Probe\nExec=mountprobe %%F\nIcon=mountprobe\nStartupWMClass=MountProbe\n' \
    > "$DIRECTORY_PAYLOAD/mountprobe.desktop"

build_synthetic_appimage "$DIRECTORY_TEMP/elf-fuse2" "$DIRECTORY_PAYLOAD" \
    "$DIRECTORY_TEMP/needs-fuse2.AppImage" gzip
build_synthetic_appimage "$DIRECTORY_TEMP/elf-static" "$DIRECTORY_PAYLOAD" \
    "$DIRECTORY_TEMP/self-contained.AppImage" gzip

echo "=== the reader says which library the runtime asks for ==="
"$DIRECTORY_BUILD/appimage-inspect" "$DIRECTORY_TEMP/needs-fuse2.AppImage" \
    > "$DIRECTORY_TEMP/inspect-fuse2.txt" 2>&1
grep -q '^fuse-library: libfuse.so.2$' "$DIRECTORY_TEMP/inspect-fuse2.txt" \
    || fail_test "the runtime's FUSE library was not found: $(grep '^fuse-library' "$DIRECTORY_TEMP/inspect-fuse2.txt")"
"$DIRECTORY_BUILD/appimage-inspect" "$DIRECTORY_TEMP/self-contained.AppImage" \
    > "$DIRECTORY_TEMP/inspect-static.txt" 2>&1
grep -q '^fuse-library: (none' "$DIRECTORY_TEMP/inspect-static.txt" \
    || fail_test "a runtime that links FUSE itself was reported as needing a library"

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

echo "=== the plan reports how the runtime starts ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan \
    "$DIRECTORY_TEMP/needs-fuse2.AppImage" > "$DIRECTORY_TEMP/plan-fuse2.txt" 2>&1
grep -q '^runtime-mount: FUSE (needs libfuse.so.2)$' "$DIRECTORY_TEMP/plan-fuse2.txt" \
    || fail_test "the plan does not report the runtime's FUSE library"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan \
    "$DIRECTORY_TEMP/self-contained.AppImage" > "$DIRECTORY_TEMP/plan-static.txt" 2>&1
grep -q '^runtime-mount: FUSE (the runtime links it itself)$' "$DIRECTORY_TEMP/plan-static.txt" \
    || fail_test "the plan misreports a runtime that links FUSE itself"

echo "=== and says when this machine cannot start it ==="
# The device and the helper are the host's, so the test asks the same question the tool
# asks and insists on the answer that matches: a machine that can mount must not be told
# it cannot, and one that cannot must be told why.
if [ -r /dev/fuse ] && [ -w /dev/fuse ]; then
    if grep -q 'mounts its payload with FUSE' "$DIRECTORY_TEMP/plan-fuse2.txt"; then
        fail_test "the plan says the AppImage cannot mount, but /dev/fuse is usable here"
    fi
    echo "  (/dev/fuse is usable here, so no such warning is expected)"
else
    grep -q 'mounts its payload with FUSE' "$DIRECTORY_TEMP/plan-fuse2.txt" \
        || fail_test "the plan did not warn that the AppImage cannot mount: $(cat "$DIRECTORY_TEMP/plan-fuse2.txt")"
    grep -q 'APPIMAGE_EXTRACT_AND_RUN=1' "$DIRECTORY_TEMP/plan-fuse2.txt" \
        || fail_test "the warning does not name the way round it"
    # A self-contained runtime is still a mount, so it gets the same warning for the
    # device, and must not name a library it does not need.
    grep -q 'mounts its payload with FUSE' "$DIRECTORY_TEMP/plan-static.txt" \
        || fail_test "a self-contained runtime still mounts, and the warning is about that"
    if grep -q 'libfuse' "$DIRECTORY_TEMP/plan-static.txt"; then
        fail_test "a self-contained runtime was told it needs libfuse"
    fi
fi

echo "=== the window's report carries it ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json \
    "$DIRECTORY_TEMP/needs-fuse2.AppImage" > "$DIRECTORY_TEMP/explain.json" 2>&1
grep -q '"fuse_libraries":\["libfuse.so.2"\]' "$DIRECTORY_TEMP/explain.json" \
    || fail_test "the JSON report does not carry the FUSE library"
grep -q '"mounts_with_fuse":true' "$DIRECTORY_TEMP/explain.json" \
    || fail_test "the JSON report does not say the runtime mounts"

pass_test "appimage fuse mount"
