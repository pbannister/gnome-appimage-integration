#!/bin/sh
#
# Tool-gated test: AppStream metadata from the payload, installed for the user.
#
# The AppImage specification ships AppStream metadata so an application is discoverable in
# application centres.  This project reads it for the version and now installs it, under
# $XDG_DATA_HOME/metainfo, named after the component id -- which is why two integrations
# of one application share a file, and why the first to be removed must not take it away.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "appstream metadata (no mksquashfs)"
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
    pass_test "appstream metadata (no host ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

# One AppImage: <name> <desktop-stem> <app-name> then the files to write under
# usr/share/metainfo, as "<file>:<component id>:<launchable>" lines on stdin.
build_with_metainfo() { # <name> <desktop-stem> <app-name> <metainfo spec...>
    name_image=$1
    stem_desktop=$2
    name_app=$3
    shift 3
    directory_payload="$DIRECTORY_TEMP/payload-$name_image"
    mkdir -p "$directory_payload/usr/share/metainfo" \
        "$directory_payload/usr/share/icons/hicolor/48x48/apps"
    printf 'png' > "$directory_payload/usr/share/icons/hicolor/48x48/apps/$stem_desktop.png"
    printf 'diricon' > "$directory_payload/.DirIcon"
    printf '[Desktop Entry]\nType=Application\nName=%s\nExec=%s %%F\nIcon=%s\n' \
        "$name_app" "$stem_desktop" "$stem_desktop" > "$directory_payload/$stem_desktop.desktop"
    for s_spec in "$@"; do
        s_file=${s_spec%%:*}
        s_rest=${s_spec#*:}
        s_id=${s_rest%%:*}
        s_launchable=${s_rest#*:}
        {
            printf '<?xml version="1.0" encoding="UTF-8"?>\n'
            printf '<component type="desktop-application">\n'
            printf '  <id>%s</id>\n' "$s_id"
            printf '  <name>%s</name>\n' "$name_app"
            printf '  <summary>a probe</summary>\n'
            printf '  <launchable type="desktop-id">%s</launchable>\n' "$s_launchable"
            printf '  <releases><release version="1.0" date="2026-01-01"/></releases>\n'
            printf '</component>\n'
        } > "$directory_payload/usr/share/metainfo/$s_file"
    done
    build_synthetic_appimage "$FILE_ELF" "$directory_payload" \
        "$DIRECTORY_TEMP/$name_image.AppImage" gzip
}

# A well-formed one, a second component for the sharing case, and two malformed shapes.
build_with_metainfo plain probe "Probe App" \
    "org.example.Probe.metainfo.xml:org.example.Probe:probe.desktop"
build_with_metainfo shared-a shareda "Shared A" \
    "org.example.Shared.metainfo.xml:org.example.Shared:shareda.desktop"
build_with_metainfo shared-b sharedb "Shared B" \
    "org.example.Shared.metainfo.xml:org.example.Shared:sharedb.desktop"
# The same component twice, in both suffixes, with a .gitkeep beside them: the modern
# suffix wins, one file is installed, and the .gitkeep is left alone.
build_with_metainfo double double "Double App" \
    "org.example.Double.appdata.xml:org.example.Double:double.desktop" \
    "org.example.Double.metainfo.xml:org.example.Double:double.desktop"
printf '' > "$DIRECTORY_TEMP/payload-double/usr/share/metainfo/.gitkeep"
# The launchable names a launcher nobody writes.
build_with_metainfo mismatch mismatch "Mismatch App" \
    "org.example.Mismatch.metainfo.xml:org.example.Mismatch:somewhere-else.desktop"

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
DIRECTORY_METAINFO="$DIRECTORY_DATA/metainfo"
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

run_tool() {
    run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" "$@"
}

echo "=== the plan says which component will be installed, and under what name ==="
run_tool plan "$DIRECTORY_TEMP/plain.AppImage" > "$DIRECTORY_TEMP/plan.txt" 2>&1
grep -q '^appstream:' "$DIRECTORY_TEMP/plan.txt" \
    || fail_test "plan does not report the AppStream metadata: $(cat "$DIRECTORY_TEMP/plan.txt")"
grep -q 'org.example.Probe.metainfo.xml -> org.example.Probe.xml' "$DIRECTORY_TEMP/plan.txt" \
    || fail_test "plan does not name the component file it would install"

echo "=== install puts it where a catalogue reads it ==="
run_tool install --yes --no-move "$DIRECTORY_TEMP/plain.AppImage" > /dev/null
if [ ! -f "$DIRECTORY_METAINFO/org.example.Probe.xml" ]; then
    fail_test "the AppStream metadata was not installed: $(ls "$DIRECTORY_METAINFO" 2>&1)"
fi
grep -q '<id>org.example.Probe</id>' "$DIRECTORY_METAINFO/org.example.Probe.xml" \
    || fail_test "the installed file is not the payload's metadata"
grep -q "appstream_package=$DIRECTORY_METAINFO/org.example.Probe.xml|" \
    "$DIRECTORY_DATA"/gnome-appimage-integration/*.manifest \
    || fail_test "the record does not name the AppStream file it wrote"
# The file name is the component id, not the payload's file name.
if [ -e "$DIRECTORY_METAINFO/org.example.Probe.metainfo.xml" ]; then
    fail_test "the metadata was installed under the payload's file name"
fi

echo "=== uninstall takes it away again ==="
identifier=$(run_tool list | awk '$3 ~ /probe.desktop$/ {print $1}' | head -1)
run_tool uninstall --identifier "$identifier" > /dev/null
if [ -e "$DIRECTORY_METAINFO/org.example.Probe.xml" ]; then
    fail_test "uninstall left the AppStream metadata behind"
fi

echo "=== one component named twice is installed once, and the .gitkeep is left alone ==="
run_tool install --yes --no-move "$DIRECTORY_TEMP/double.AppImage" > /dev/null
if [ ! -f "$DIRECTORY_METAINFO/org.example.Double.xml" ]; then
    fail_test "the doubled component was not installed"
fi
count_double=$(find "$DIRECTORY_METAINFO" -name 'org.example.Double*' | wc -l)
if [ "$count_double" != "1" ]; then
    fail_test "the component was installed $count_double times, not once"
fi
if [ -e "$DIRECTORY_METAINFO/.gitkeep" ]; then
    fail_test "a file that is not AppStream metadata was installed"
fi
identifier=$(run_tool list | awk '$3 ~ /double.desktop$/ {print $1}' | head -1)
run_tool uninstall --identifier "$identifier" > /dev/null

echo "=== a component two integrations share survives the first of them going ==="
run_tool install --yes --no-move "$DIRECTORY_TEMP/shared-a.AppImage" > /dev/null
run_tool install --yes --no-move "$DIRECTORY_TEMP/shared-b.AppImage" > /dev/null
if [ ! -f "$DIRECTORY_METAINFO/org.example.Shared.xml" ]; then
    fail_test "the shared component was not installed"
fi
identifier_a=$(run_tool list | awk '$3 ~ /shareda.desktop$/ {print $1}' | head -1)
identifier_b=$(run_tool list | awk '$3 ~ /sharedb.desktop$/ {print $1}' | head -1)
if [ -z "$identifier_a" ] || [ -z "$identifier_b" ]; then
    fail_test "the two integrations were not both recorded"
fi
run_tool uninstall --identifier "$identifier_a" > /dev/null
if [ ! -f "$DIRECTORY_METAINFO/org.example.Shared.xml" ]; then
    fail_test "the first uninstall removed a component the other integration still needs"
fi
run_tool uninstall --identifier "$identifier_b" > /dev/null
if [ -e "$DIRECTORY_METAINFO/org.example.Shared.xml" ]; then
    fail_test "the last uninstall left the component behind"
fi
if [ -e "$DIRECTORY_DATA/gnome-appimage-integration/backup/appstream-org.example.Shared.xml" ]; then
    fail_test "the shared component left a backup behind"
fi

echo "=== a file that was already there is put back, not deleted ==="
mkdir -p "$DIRECTORY_METAINFO"
printf '<component><id>org.example.Foreign</id><name>the distribution has this one</name></component>' \
    > "$DIRECTORY_METAINFO/org.example.Foreign.xml"
# A payload whose component id is the one already on disk.
build_with_metainfo foreign foreign "Foreign App" \
    "foreign.metainfo.xml:org.example.Foreign:foreign.desktop"
run_tool install --yes --no-move "$DIRECTORY_TEMP/foreign.AppImage" > /dev/null
if grep -q 'the distribution has this one' "$DIRECTORY_METAINFO/org.example.Foreign.xml"; then
    fail_test "the payload's metadata did not replace the file that was there"
fi
identifier=$(run_tool list | awk '$3 ~ /foreign.desktop$/ {print $1}' | head -1)
run_tool uninstall --identifier "$identifier" > /dev/null
if ! grep -q 'the distribution has this one' "$DIRECTORY_METAINFO/org.example.Foreign.xml" 2>/dev/null; then
    fail_test "uninstall did not put back the file that was there before"
fi

echo "=== a launchable nobody writes is reported ==="
run_tool plan "$DIRECTORY_TEMP/mismatch.AppImage" > "$DIRECTORY_TEMP/mismatch.txt" 2>&1
grep -q 'says it launches somewhere-else.desktop' "$DIRECTORY_TEMP/mismatch.txt" \
    || fail_test "a launchable that names no launcher was not reported: $(cat "$DIRECTORY_TEMP/mismatch.txt")"

pass_test "appstream metadata"
