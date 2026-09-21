#!/bin/sh
#
# Live-state test: verify the documented desktop-file search path against reality.
#
# Prerequisite: run inside a real desktop session (a GNOME session is expected).
# It reads $HOME, $XDG_DATA_HOME, $XDG_DATA_DIRS, $XDG_CONFIG_HOME, $XDG_CONFIG_DIRS,
# and $XDG_CURRENT_DESKTOP, and inspects whatever desktop files exist on this host.
# It reports PASS, WARN, or FAIL and exits non-zero only on FAIL.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

FILE_DESKTOP_INSPECT="$DIRECTORY_BUILD/desktop-inspect"
COUNT_FAIL=0
COUNT_WARN=0

fail() {
    echo "FAIL: $1" >&2
    COUNT_FAIL=$((COUNT_FAIL + 1))
}

warn() {
    echo "WARN: $1" >&2
    COUNT_WARN=$((COUNT_WARN + 1))
}

DIRECTORY_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
DIRECTORY_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
CONFIG_DIRS=${XDG_CONFIG_DIRS:-/etc/xdg}

# The locator normalizes a trailing slash (as GLib does), so normalize here too.
DIRECTORY_DATA_HOME=$(printf '%s' "$DIRECTORY_DATA_HOME" | sed 's:/*$::')
DIRECTORY_CONFIG_HOME=$(printf '%s' "$DIRECTORY_CONFIG_HOME" | sed 's:/*$::')

# --- The application search path must match the documented derivation. ---
OUTPUT_PATH=$("$FILE_DESKTOP_INSPECT" --path)
LINE_FIRST=$(printf '%s\n' "$OUTPUT_PATH" | head -1)
if [ "$LINE_FIRST" != "$DIRECTORY_DATA_HOME/applications" ]; then
    fail "first search directory is $LINE_FIRST, expected $DIRECTORY_DATA_HOME/applications"
fi

OLD_IFS=$IFS
IFS=:
SEQUENCE_EXPECTED="$DIRECTORY_DATA_HOME/applications"
for directory_data in $DATA_DIRS; do
    if [ -n "$directory_data" ]; then
        directory_data=$(printf '%s' "$directory_data" | sed 's:/*$::')
        SEQUENCE_EXPECTED="$SEQUENCE_EXPECTED
$directory_data/applications"
    fi
done
IFS=$OLD_IFS

LINE_LAST=0
while IFS= read -r directory_expected; do
    [ -n "$directory_expected" ] || continue
    LINE_CURRENT=$(printf '%s\n' "$OUTPUT_PATH" | awk -v target="$directory_expected" '$0 == target { print NR; exit }')
    if [ -z "$LINE_CURRENT" ]; then
        fail "missing search directory: $directory_expected"
    elif [ "$LINE_CURRENT" -le "$LINE_LAST" ]; then
        fail "search directory out of order: $directory_expected"
    else
        LINE_LAST=$LINE_CURRENT
    fi
done <<EOF
$SEQUENCE_EXPECTED
EOF

# --- The autostart search path must match the documented derivation. ---
OUTPUT_AUTOSTART=$("$FILE_DESKTOP_INSPECT" --autostart-path)
LINE_AUTOSTART_FIRST=$(printf '%s\n' "$OUTPUT_AUTOSTART" | head -1)
if [ "$LINE_AUTOSTART_FIRST" != "$DIRECTORY_CONFIG_HOME/autostart" ]; then
    fail "first autostart directory is $LINE_AUTOSTART_FIRST, expected $DIRECTORY_CONFIG_HOME/autostart"
fi

# --- At least one application directory must exist on a real session. ---
COUNT_EXISTING=0
while IFS= read -r directory_application; do
    [ -n "$directory_application" ] || continue
    if [ -d "$directory_application" ]; then
        COUNT_EXISTING=$((COUNT_EXISTING + 1))
    fi
done <<EOF
$OUTPUT_PATH
EOF
if [ "$COUNT_EXISTING" -eq 0 ]; then
    warn "no application directory in the search path exists"
fi

# --- Enumeration must find entries when directories exist. ---
OUTPUT_ALL=$("$FILE_DESKTOP_INSPECT" --all)
COUNT_ENTRIES=$(printf '%s\n' "$OUTPUT_ALL" | grep -c . || true)
if [ "$COUNT_EXISTING" -gt 0 ] && [ "$COUNT_ENTRIES" -eq 0 ]; then
    warn "application directories exist but no desktop entry was enumerated"
fi

# --- GNOME session check. ---
case "${XDG_CURRENT_DESKTOP:-}" in
    *GNOME*) ;;
    *) warn "XDG_CURRENT_DESKTOP does not name GNOME: ${XDG_CURRENT_DESKTOP:-<unset>}" ;;
esac

if [ "$COUNT_FAIL" -gt 0 ]; then
    echo "50-desktop-search-path-live: FAIL ($COUNT_FAIL failures, $COUNT_WARN warnings)"
    exit 1
fi
echo "50-desktop-search-path-live: PASS ($COUNT_EXISTING existing directories, $COUNT_ENTRIES entries, $COUNT_WARN warnings)"
exit 0
