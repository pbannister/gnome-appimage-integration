#!/bin/sh
#
# live-state-reset.sh: return this desktop's AppImage integration to a clean start.
#
# It writes a restore point first (scripts/live-state-snapshot.sh), then uninstalls every
# integration this tool recorded, then sweeps what a run leaves behind but no record
# names any more -- orphaned launchers, icons, and per-AppImage MIME definitions from
# earlier runs.  The AppImage files themselves stay where they are: only the desktop
# integration is reset.
#
# The *.AppImage handler is deliberately left registered.  It is a desktop registration
# the owner chose, not per-run state, and `handler uninstall` is its own command.
#
# Files that belong to another tool are left alone: `appimagekit_*` MIME definitions and
# the shared `appimage.xml` are not this tool's to remove.
#
# Usage:
#   sh scripts/live-state-reset.sh [--dry-run]
#
# Environment:
#   APPIMAGE_CORPUS_DIR   where corpus and live-state work lives (default ~/appimage-corpus)
#   APPIMAGE_INTEGRATE    the tool to use (default: the installed one)
#   NO_SNAPSHOT=1         do not write a restore point (only when one already exists)
#   XDG_DATA_HOME         where the desktop data lives (default ~/.local/share)
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRECTORY_DATA=${XDG_DATA_HOME:-$HOME/.local/share}
DIRECTORY_STATE="$DIRECTORY_DATA/gnome-appimage-integration"
DIRECTORY_APPLICATIONS="$DIRECTORY_DATA/applications"
DIRECTORY_ICONS="$DIRECTORY_DATA/icons/hicolor"
DIRECTORY_MIME="$DIRECTORY_DATA/mime/packages"
DIRECTORY_WORK=${APPIMAGE_CORPUS_DIR:-$HOME/appimage-corpus}

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
fi

say() {
    echo "live-state-reset: $1"
}

fail() {
    echo "live-state-reset: $1" >&2
    exit 1
}

run() {
    if [ 1 -eq "$DRY_RUN" ]; then
        echo "  would run: $*"
        return 0
    fi
    "$@"
}

remove() { # <path>
    [ -e "$1" ] || return 0
    echo "  remove: $1"
    [ 1 -eq "$DRY_RUN" ] || rm -rf "$1"
}

# ---------------------------------------------------------------------------
# The tool

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
say "using $TOOL"

# ---------------------------------------------------------------------------
# A restore point first.  A reset without one is not recoverable, so this stops if the
# snapshot cannot be written, and refuses to skip the snapshot when integrations exist
# and no earlier restore point does.

if [ "1" = "${NO_SNAPSHOT:-0}" ]; then
    say "no restore point written, on request (NO_SNAPSHOT=1)"
else
    if [ 1 -eq "$DRY_RUN" ]; then
        run sh "$DIRECTORY_SCRIPT/live-state-snapshot.sh"
    else
        sh "$DIRECTORY_SCRIPT/live-state-snapshot.sh" \
            || fail "the restore point could not be written; nothing was reset"
        say "a restore point was written before the reset"
    fi
fi

# ---------------------------------------------------------------------------
# Uninstall every recorded integration.

count_record=0
list_failed=
if [ -d "$DIRECTORY_STATE" ]; then
    for file_manifest in "$DIRECTORY_STATE"/*.manifest; do
        [ -f "$file_manifest" ] || continue
        identifier=$(sed -n 's/^identifier=//p' "$file_manifest" | head -1)
        [ -n "$identifier" ] || continue
        count_record=$((count_record + 1))
        if [ 1 -eq "$DRY_RUN" ]; then
            echo "  would uninstall: $identifier"
            continue
        fi
        file_log="$DIRECTORY_WORK/live-state/uninstall-$identifier.log"
        mkdir -p "$(dirname "$file_log")"
        if ! "$TOOL" uninstall --yes --identifier "$identifier" > "$file_log" 2>&1; then
            list_failed="$list_failed $identifier"
            echo "  uninstall failed: $identifier (see $file_log)" >&2
        else
            echo "  uninstalled: $identifier"
        fi
    done
fi
say "$count_record recorded integration(s)"

# ---------------------------------------------------------------------------
# Sweep what earlier runs left behind: anything inside this tool's own namespaces that
# no record names any more.  Each pattern is narrow on purpose.

count_swept=0
sweep() { # <description> <find-expression...>
    description=$1
    shift
    for path_swept in $(find "$@" 2>/dev/null | sort); do
        count_swept=$((count_swept + 1))
        remove "$path_swept"
    done
}

# Launchers this tool wrote but no record claims (a run that was interrupted, or a
# record that was replaced).
if [ -d "$DIRECTORY_APPLICATIONS" ]; then
    for file_launcher in "$DIRECTORY_APPLICATIONS"/*.desktop; do
        [ -f "$file_launcher" ] || continue
        if grep -q '^X-Integrated-By=gnome-appimage-integration$' "$file_launcher" 2>/dev/null; then
            count_swept=$((count_swept + 1))
            remove "$file_launcher"
        fi
    done
fi

# Per-AppImage MIME definitions: <16 hex>[_<name>].xml, and the older <hex>-<n>_<name>.xml.
sweep "per-AppImage MIME definitions" "$DIRECTORY_MIME" -maxdepth 1 -type f \
    \( -name '[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]_*.xml' \
    -o -name '[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]-*_*.xml' \)

# Icons: appimage_<hex>_<name>.<ext>, the name every integration's icons carry.
sweep "integration icons" "$DIRECTORY_ICONS" -type f -name 'appimage_[0-9a-f]*'

# Records and the backup of launchers they displaced.  ui.json is the window's size and
# position, which is not integration state, so it stays.
sweep "records" "$DIRECTORY_STATE" -maxdepth 1 -type f -name '*.manifest'
if [ -d "$DIRECTORY_STATE/backup" ]; then
    sweep "displaced-launcher backups" "$DIRECTORY_STATE/backup" -type f
fi

say "$count_swept leftover file(s)"

# ---------------------------------------------------------------------------
# Let the desktop see the change.

if [ 1 -eq "$DRY_RUN" ]; then
    say "dry run: nothing was written"
    exit 0
fi

if command -v update-mime-database >/dev/null 2>&1; then
    update-mime-database "$DIRECTORY_DATA/mime" >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DIRECTORY_APPLICATIONS" >/dev/null 2>&1 || true
fi
if command -v gtk4-update-icon-cache >/dev/null 2>&1; then
    gtk4-update-icon-cache -f -t "$DIRECTORY_ICONS" >/dev/null 2>&1 || true
elif command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$DIRECTORY_ICONS" >/dev/null 2>&1 || true
fi

say "the desktop now has:"
"$TOOL" list 2>&1 | sed 's/^/  /' || true

if [ -n "$list_failed" ]; then
    fail "these integrations could not be uninstalled:$list_failed"
fi
