#!/bin/sh
#
# live-state-snapshot.sh: write a restore point for every AppImage this tool has
# integrated into the desktop, so the state can be reset for testing and put back
# afterwards.
#
# The restore point is a tab-separated table, one row per integration, holding what
# `install` needs to reproduce it: the managed file, the desktop id, the icon name, the
# window class and whether it was chosen, the Name= the launcher carries, and where the
# file lives.  It is written outside the state the reset removes, so resetting cannot
# destroy it.
#
# Usage:
#   sh scripts/live-state-snapshot.sh
#
# Environment:
#   APPIMAGE_CORPUS_DIR   where the corpus and its restore points live (default:
#                         /datasets/appimage-corpus when /datasets exists, else
#                         ~/appimage-corpus)
#   APPIMAGE_INTEGRATE    the tool to use (default: the installed one)
#   XDG_DATA_HOME         where the desktop data lives (default ~/.local/share)
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$DIRECTORY_SCRIPT/lib/corpus-paths.sh"

DIRECTORY_DATA=${XDG_DATA_HOME:-$HOME/.local/share}
DIRECTORY_STATE="$DIRECTORY_DATA/gnome-appimage-integration"
DIRECTORY_APPLICATIONS="$DIRECTORY_DATA/applications"
DIRECTORY_WORK=$(corpus_directory)
DIRECTORY_RESTORE="$DIRECTORY_WORK/live-state"

say() {
    echo "live-state-snapshot: $1"
}

fail() {
    echo "live-state-snapshot: $1" >&2
    exit 1
}

# Read one key from a manifest, first match only.
manifest_value() { # <manifest> <key>
    sed -n "s/^$2=//p" "$1" | head -1
}

# Read Name= out of a launcher, or nothing.
launcher_name() { # <desktop-file>
    [ -f "$1" ] || return 0
    sed -n 's/^Name=//p' "$1" | head -1
}

mkdir -p "$DIRECTORY_RESTORE"
FILE_RESTORE="$DIRECTORY_RESTORE/restore-point-$(date +%Y%m%d-%H%M%S).tsv"

count_integration=0
size_total=0

{
    printf '# gnome-appimage-integration restore point\n'
    printf '# written %s\n' "$(date -Iseconds)"
    printf '# identifier\tappimage_path\tsource_path\tdesktop_id\tdesktop_entry\ticon_name\tstartup_wm_class\tstartup_wm_class_source\tname\tinstall_dir\n'

    if [ -d "$DIRECTORY_STATE" ]; then
        for file_manifest in "$DIRECTORY_STATE"/*.manifest; do
            [ -f "$file_manifest" ] || continue
            identifier=$(manifest_value "$file_manifest" identifier)
            appimage_path=$(manifest_value "$file_manifest" appimage_path)
            [ -n "$identifier" ] || continue
            [ -n "$appimage_path" ] || continue

            source_path=$(manifest_value "$file_manifest" source_path)
            desktop_id=$(manifest_value "$file_manifest" desktop_id)
            desktop_entry=$(manifest_value "$file_manifest" desktop_entry)
            icon_name=$(manifest_value "$file_manifest" icon_name)
            wm_class=$(manifest_value "$file_manifest" startup_wm_class)
            wm_source=$(manifest_value "$file_manifest" startup_wm_class_source)
            name=$(launcher_name "$desktop_entry")

            # install_dir is where the file must be placed; the default is ~/Applications,
            # and only a different one has to be passed back.
            install_dir=$(dirname "$appimage_path")

            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                "$identifier" "$appimage_path" "$source_path" "$desktop_id" \
                "$desktop_entry" "$icon_name" "$wm_class" "$wm_source" "$name" "$install_dir"

            count_integration=$((count_integration + 1))
            if [ -f "$appimage_path" ]; then
                size_total=$((size_total + $(wc -c < "$appimage_path")))
            fi
        done
    fi
} > "$FILE_RESTORE"

if [ "$count_integration" -eq 0 ]; then
    say "no integration is recorded; the restore point is empty"
fi

say "$count_integration integration(s), $(awk -v s="$size_total" 'BEGIN{printf "%.2f", s/1000000000}') GB of AppImages"
say "restore point: $FILE_RESTORE"
