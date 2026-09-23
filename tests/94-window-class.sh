#!/bin/sh
#
# Tool-gated test: reading StartupWMClass from the running application.
#
# GNOME does not let another program list windows, so the tool reads what it can:
# the processes under a mounted AppImage (their program name is the id GTK reports
# for a window when the application sets no GtkApplication id), and X11 or XWayland
# clients, whose WM_CLASS is exactly what the dock matches on.
#
# Prerequisites: mksquashfs and a 64-bit host ELF for the synthetic AppImage;
# Xvfb, xlsclients and xprop for the window half of the test.  Everything runs
# against temporary files and a temporary HOME; the real desktop is untouched.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "window class discovery (no mksquashfs)"
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
    pass_test "window class discovery (no host ELF)"
    exit 0
fi

FILE_SLEEP=$(command -v sleep || true)
FILE_BASH=$(command -v bash || true)
if [ -z "$FILE_SLEEP" ] || [ -z "$FILE_BASH" ]; then
    echo "SKIP: sleep or bash is not available"
    pass_test "window class discovery (no sleep or bash)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
PID_PROBE=
cleanup() {
    if [ -n "$PID_PROBE" ]; then
        kill "$PID_PROBE" 2>/dev/null || true
    fi
    rm -rf "$DIRECTORY_TEMP"
}
trap cleanup EXIT

# A temporary HOME, so a class that happens to match a launcher on the real desktop
# cannot change the answer.
DIRECTORY_HOME="$DIRECTORY_TEMP/home"
mkdir -p "$DIRECTORY_HOME/.local/share/applications" "$DIRECTORY_TEMP/share" \
    "$DIRECTORY_HOME/.config" "$DIRECTORY_TEMP/etc"
export HOME="$DIRECTORY_HOME"
export XDG_DATA_HOME="$DIRECTORY_HOME/.local/share"
export XDG_DATA_DIRS="$DIRECTORY_TEMP/share:/usr/share"
export XDG_CONFIG_HOME="$DIRECTORY_HOME/.config"
export XDG_CONFIG_DIRS="$DIRECTORY_TEMP/etc"

# A synthetic AppImage whose embedded entry names a class the dock would not match;
# the runtime class has to come from the running application instead.
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
StartupWMClass=ProbeEmbedded
ENTRY

FILE_APPIMAGE="$DIRECTORY_TEMP/Probe.AppImage"
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$FILE_APPIMAGE" gzip

# A program inside a mounted AppImage: this is the layout a real AppImage has, and
# the program name is what GTK falls back to as the window's application id.
DIRECTORY_MOUNT="$DIRECTORY_TEMP/.mount_probeABC12/usr/bin"
mkdir -p "$DIRECTORY_MOUNT"
cp "$FILE_SLEEP" "$DIRECTORY_MOUNT/orca-slicer"
FILE_IN_MOUNT="$DIRECTORY_MOUNT/orca-slicer"

echo "=== windows must always answer ==="
"$DIRECTORY_BUILD/appimage-integrate" windows --json > "$DIRECTORY_TEMP/empty.json" 2>&1
FIRST_CHARACTER=$(cut -c 1 "$DIRECTORY_TEMP/empty.json")
if [ "$FIRST_CHARACTER" != "[" ]; then
    fail_test "windows --json did not print a JSON array"
fi
"$DIRECTORY_BUILD/appimage-integrate" windows > "$DIRECTORY_TEMP/empty.txt" 2>&1 \
    || fail_test "windows failed when nothing was running"

echo "=== a process inside a mounted AppImage is found ==="
# argv[0] is the AppImage path, the way the AppImage runtime starts its payload.
"$FILE_BASH" -c 'exec -a "$1" "$2" 60' probe "$FILE_APPIMAGE" "$FILE_IN_MOUNT" &
PID_PROBE=$!
sleep 1

"$DIRECTORY_BUILD/appimage-integrate" windows > "$DIRECTORY_TEMP/windows.txt" 2>&1
grep -q -- '--wm-class orca-slicer' "$DIRECTORY_TEMP/windows.txt" \
    || fail_test "windows did not offer the program name as the window class"
grep -q "$FILE_IN_MOUNT" "$DIRECTORY_TEMP/windows.txt" \
    || fail_test "windows did not report the program inside the mount"
grep -q "the program name, which GTK reports as the window app id" "$DIRECTORY_TEMP/windows.txt" \
    || fail_test "windows did not explain where the class came from"

"$DIRECTORY_BUILD/appimage-integrate" windows --json > "$DIRECTORY_TEMP/windows.json" 2>&1
grep -q "\"appimage\":\"$FILE_APPIMAGE\"" "$DIRECTORY_TEMP/windows.json" \
    || fail_test "windows --json did not report the AppImage"
grep -q '"process":"orca-slicer"' "$DIRECTORY_TEMP/windows.json" \
    || fail_test "windows --json did not report the process name"
grep -q '"executable":"'"$FILE_IN_MOUNT"'"' "$DIRECTORY_TEMP/windows.json" \
    || fail_test "windows --json did not report the executable"

echo "=== the class is applied with --wm-class-from-window ==="
"$DIRECTORY_BUILD/appimage-integrate" plan --wm-class-from-window "$FILE_APPIMAGE" \
    > "$DIRECTORY_TEMP/plan.txt" 2> "$DIRECTORY_TEMP/plan.err"
grep -q 'StartupWMClass orca-slicer read from process' "$DIRECTORY_TEMP/plan.err" \
    || fail_test "the read class was not reported"
grep -q 'startup-wm-class: orca-slicer' "$DIRECTORY_TEMP/plan.txt" \
    || fail_test "the read class did not reach the plan"
grep -q 'kept over the embedded entry' "$DIRECTORY_TEMP/plan.txt" \
    || fail_test "the plan does not say the read class outranks the embedded one"

echo "=== an unknown AppImage fails with the candidates ==="
if "$DIRECTORY_BUILD/appimage-integrate" plan --wm-class-from-window \
        "$DIRECTORY_TEMP/Other.AppImage" > "$DIRECTORY_TEMP/missing.txt" 2>&1; then
    fail_test "--wm-class-from-window succeeded for an AppImage that is not running"
fi
grep -q 'no running process belongs to' "$DIRECTORY_TEMP/missing.txt" \
    || fail_test "the failure does not say which AppImage was not found"
grep -q "$FILE_APPIMAGE" "$DIRECTORY_TEMP/missing.txt" \
    || fail_test "the failure does not list the running AppImages"

echo "=== a class chosen once outlives a re-integration ==="
# Install, choosing a class by hand, then install again: the embedded value must not
# take the class back, because the embedded value is the one that did not match.
"$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes --wm-class OrcaClass \
    "$FILE_APPIMAGE" > "$DIRECTORY_TEMP/install.txt" 2>&1
grep -q '^StartupWMClass=OrcaClass$' "$DIRECTORY_HOME/.local/share/applications/org.example.Probe.desktop" \
    || fail_test "the explicit class was not written"
grep -q '^startup_wm_class_source=override$' "$DIRECTORY_HOME/.local/share/gnome-appimage-integration/"*.manifest \
    || fail_test "the manifest does not record that the class was chosen"
"$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes "$FILE_APPIMAGE" \
    > "$DIRECTORY_TEMP/reinstall.txt" 2>&1
grep -q '^StartupWMClass=OrcaClass$' "$DIRECTORY_HOME/.local/share/applications/org.example.Probe.desktop" \
    || fail_test "re-integrating let the embedded class win back"
"$DIRECTORY_BUILD/appimage-integrate" install --no-move --yes "$FILE_APPIMAGE" \
    > "$DIRECTORY_TEMP/reinstall2.txt" 2>&1
grep -q 'kept StartupWMClass=OrcaClass, which an earlier install set explicitly' \
    "$DIRECTORY_TEMP/reinstall2.txt" \
    || fail_test "the re-integration does not say it kept the earlier choice"
"$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier \
    "$("$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}')" > /dev/null 2>&1

echo "=== an X11 window's WM_CLASS is preferred ==="
if ! command -v xvfb-run >/dev/null 2>&1 || ! command -v xlsclients >/dev/null 2>&1 \
    || ! command -v xprop >/dev/null 2>&1; then
    echo "SKIP: xvfb-run, xlsclients or xprop is not available"
elif [ ! -x "$DIRECTORY_BUILD/appimage-activator" ]; then
    echo "SKIP: appimage-activator was not built (libgtk-4-dev is missing)"
else
    # The activator is started with the same AppImage path as argv[0] as the mount
    # probe above, so that AppImage has both a process and a window.  The window's
    # class is the one the dock matches on, so it must win over the program name.
    xvfb-run -a env \
        PROBE_APPIMAGE="$FILE_APPIMAGE" \
        PROBE_ACTIVATOR="$DIRECTORY_BUILD/appimage-activator" \
        PROBE_TOOL="$DIRECTORY_BUILD/appimage-integrate" \
        PROBE_LOG="$DIRECTORY_TEMP/activator.log" \
        PROBE_WINDOWS="$DIRECTORY_TEMP/x11-windows.txt" \
        PROBE_PLAN="$DIRECTORY_TEMP/x11-plan.txt" \
        "$FILE_BASH" -c '
            export GDK_BACKEND=x11
            exec -a "$PROBE_APPIMAGE" "$PROBE_ACTIVATOR" --tool "$PROBE_TOOL" \
                "$PROBE_APPIMAGE" > "$PROBE_LOG" 2>&1 &
            PID_ACTIVATOR=$!
            sleep 3
            "$PROBE_TOOL" windows > "$PROBE_WINDOWS" 2>&1 || true
            "$PROBE_TOOL" plan --wm-class-from-window "$PROBE_APPIMAGE" \
                > "$PROBE_PLAN" 2>&1 || true
            kill "$PID_ACTIVATOR" 2>/dev/null || true
        '

    grep -q 'appimage-activator' "$DIRECTORY_TEMP/x11-windows.txt" \
        || fail_test "windows did not report the X11 window's class"
    grep -q -- '--wm-class appimage-activator' "$DIRECTORY_TEMP/x11-windows.txt" \
        || fail_test "the X11 class was not offered as --wm-class"
    grep -q 'read from the window' "$DIRECTORY_TEMP/x11-plan.txt" \
        || fail_test "the X11 class was not read from the window"
    grep -q 'startup-wm-class: appimage-activator' "$DIRECTORY_TEMP/x11-plan.txt" \
        || fail_test "the X11 class did not reach the plan"
fi

pass_test "window class discovery"
