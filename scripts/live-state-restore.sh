#!/bin/sh
#
# live-state-restore.sh: put back the integrations a restore point recorded.
#
# Reads a restore point written by scripts/live-state-snapshot.sh (newest by default) and
# re-integrates each AppImage with the options that were in force: the desktop id, the
# icon name, the Name= the launcher carried, the window class when it had been chosen by
# hand, and the directory the file lives in.  Each restored launcher is then checked
# against the record.
#
# Usage:
#   sh scripts/live-state-restore.sh [restore-point.tsv]
#
# Environment:
#   APPIMAGE_CORPUS_DIR   where the corpus and its restore points live (default:
#                         /datasets/appimage-corpus when /datasets exists, else
#                         ~/appimage-corpus)
#   APPIMAGE_INTEGRATE    the tool to use (default: the installed one)
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$DIRECTORY_SCRIPT/lib/corpus-paths.sh"

DIRECTORY_WORK=$(corpus_directory)
DIRECTORY_RESTORE="$DIRECTORY_WORK/live-state"
TAB=$(printf '\t')

say() {
    echo "live-state-restore: $1"
}

fail() {
    echo "live-state-restore: $1" >&2
    exit 1
}

TOOL=${APPIMAGE_INTEGRATE:-}
if [ -z "$TOOL" ]; then
    for candidate_tool in "$HOME/.local/bin/appimage-integrate" \
        "$(command -v appimage-integrate 2>/dev/null || true)"; do
        if [ -n "$candidate_tool" ] && [ -x "$candidate_tool" ]; then
            TOOL=$candidate_tool
            break
        fi
    done
fi
[ -n "$TOOL" ] || fail "appimage-integrate was not found; set APPIMAGE_INTEGRATE"

FILE_RESTORE=${1:-}
if [ -z "$FILE_RESTORE" ]; then
    FILE_RESTORE=$(ls -1t "$DIRECTORY_RESTORE"/restore-point-*.tsv 2>/dev/null | head -1 || true)
fi
[ -n "$FILE_RESTORE" ] || fail "no restore point in $DIRECTORY_RESTORE; pass one as an argument"
[ -f "$FILE_RESTORE" ] || fail "no such restore point: $FILE_RESTORE"
say "restoring from $FILE_RESTORE"

# The launcher must carry what the record says it carried.
verify() { # <desktop-entry> <name> <icon-name> <wm-class> <appimage-path>
    file_entry=$1
    if [ ! -f "$file_entry" ]; then
        echo "  launcher was not written: $file_entry" >&2
        return 1
    fi
    status_verify=0
    [ -z "$2" ] || grep -qxF "Name=$2" "$file_entry" || { echo "  Name= is not $2" >&2; status_verify=1; }
    [ -z "$3" ] || grep -qxF "Icon=$3" "$file_entry" || { echo "  Icon= is not $3" >&2; status_verify=1; }
    [ -z "$4" ] || grep -qxF "StartupWMClass=$4" "$file_entry" || { echo "  StartupWMClass= is not $4" >&2; status_verify=1; }
    grep -qF "$5" "$file_entry" || { echo "  Exec= does not name $5" >&2; status_verify=1; }
    return $status_verify
}

count_restored=0
count_failed=0
# The row is split with cut rather than `read`: a tab is IFS whitespace, so `read` treats a
# run of tabs as one delimiter and swallows the empty fields between them, which shifts
# every column after the first two empty ones -- exactly what an integration with no
# window class records.
while IFS= read -r line_row; do
    case "$line_row" in
        '' | '#'*) continue ;;
    esac
    identifier=$(printf '%s\n' "$line_row" | cut -f1)
    appimage_path=$(printf '%s\n' "$line_row" | cut -f2)
    source_path=$(printf '%s\n' "$line_row" | cut -f3)
    desktop_id=$(printf '%s\n' "$line_row" | cut -f4)
    desktop_entry=$(printf '%s\n' "$line_row" | cut -f5)
    icon_name=$(printf '%s\n' "$line_row" | cut -f6)
    wm_class=$(printf '%s\n' "$line_row" | cut -f7)
    wm_source=$(printf '%s\n' "$line_row" | cut -f8)
    name=$(printf '%s\n' "$line_row" | cut -f9)
    install_dir=$(printf '%s\n' "$line_row" | cut -f10)

    if [ ! -f "$appimage_path" ]; then
        echo "  missing AppImage, skipped: $appimage_path" >&2
        count_failed=$((count_failed + 1))
        continue
    fi

    set -- install --yes \
        --desktop-file-name "$desktop_id" \
        --icon-name "$icon_name"
    [ -z "$name" ] || set -- "$@" --name "$name"
    # A class that was chosen is passed back; one taken from the embedded entry is left to
    # be taken from it again, which keeps the record's source honest.  A record that names
    # a class without naming a source -- what an older version of this tool wrote -- would
    # otherwise lose the class entirely, so that case passes it back too.
    if [ -n "$wm_class" ]; then
        if [ "$wm_source" = "override" ] || [ -z "$wm_source" ]; then
            set -- "$@" --wm-class "$wm_class"
        fi
    fi
    if [ "$install_dir" != "$HOME/Applications" ]; then
        set -- "$@" --install-dir "$install_dir"
    fi
    set -- "$@" "$appimage_path"

    if ! "$TOOL" "$@" > "$DIRECTORY_RESTORE/restore-$identifier.log" 2>&1; then
        echo "  install failed: $identifier (see $DIRECTORY_RESTORE/restore-$identifier.log)" >&2
        count_failed=$((count_failed + 1))
        continue
    fi
    if verify "$desktop_entry" "$name" "$icon_name" "$wm_class" "$appimage_path"; then
        echo "  restored: $identifier  $desktop_id"
        count_restored=$((count_restored + 1))
    else
        echo "  restored with differences: $identifier" >&2
        count_failed=$((count_failed + 1))
    fi
done < "$FILE_RESTORE"

say "$count_restored restored, $count_failed failed"
[ "$count_failed" -eq 0 ] || exit 1
