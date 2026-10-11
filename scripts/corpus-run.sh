#!/bin/sh
#
# corpus-run.sh: run the tools against every AppImage in the corpus and catalogue how
# each one is shaped and what the tools made of it.
#
# Every application gets its own throwaway HOME and XDG tree under the corpus directory,
# created fresh for it and removed afterwards.  Nothing is inherited: not from an earlier
# application, not from an earlier run, and not from the real desktop.  `plan` writes
# nothing anywhere; `install --no-move` writes only inside that throwaway tree, and the
# corpus file itself is never moved.
#
# Output, under $APPIMAGE_CORPUS_DIR/reports/:
#   <app>/inspect.txt, <app>/plan.txt, <app>/install.txt   the raw output, kept
#   summary.tsv                                            one row per application
#   edge-cases.txt                                         the distinct findings, counted
#
# Usage:
#   sh scripts/corpus-run.sh [app ...]        (default: every fetched application)
#
# Environment:
#   APPIMAGE_CORPUS_DIR   where the corpus lives (default ~/appimage-corpus)
#   APPIMAGE_INTEGRATE    the tool under test (default: the build tree, then the installed one)
#   APPIMAGE_INSPECT      likewise for appimage-inspect
#   KEEP_SANDBOX=1        keep each application's sandbox for inspection
#   SKIP_INSTALL=1        plan only, do not exercise install
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DIRECTORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_WORK=${APPIMAGE_CORPUS_DIR:-$HOME/appimage-corpus}
DIRECTORY_APPIMAGES="$DIRECTORY_WORK/appimages"
DIRECTORY_REPORTS="$DIRECTORY_WORK/reports"
FILE_FETCHED="$DIRECTORY_WORK/corpus/fetched.tsv"
TAB=$(printf '\t')

say() {
    echo "corpus-run: $1"
}

fail() {
    echo "corpus-run: $1" >&2
    exit 1
}

# Prefer what this checkout builds: the corpus is here to test the working tree, not the
# copy installed on the machine.
resolve_tool() { # <name> <environment override>
    value=$(eval "printf '%s' \"\${$2:-}\"")
    if [ -n "$value" ]; then
        printf '%s' "$value"
        return 0
    fi
    if [ -x "$DIRECTORY_ROOT/dataflow.out/build/$1" ]; then
        printf '%s' "$DIRECTORY_ROOT/dataflow.out/build/$1"
        return 0
    fi
    command -v "$1" 2>/dev/null || true
}

TOOL=$(resolve_tool appimage-integrate APPIMAGE_INTEGRATE)
INSPECT=$(resolve_tool appimage-inspect APPIMAGE_INSPECT)
[ -n "$TOOL" ] || fail "appimage-integrate was not found; build it or set APPIMAGE_INTEGRATE"
[ -n "$INSPECT" ] || fail "appimage-inspect was not found; build it or set APPIMAGE_INSPECT"

mkdir -p "$DIRECTORY_REPORTS"
say "tool:    $TOOL"
say "inspect: $INSPECT"

# The applications to run: the names given, or every fetched one.
DIRECTORY_SELECTED="$DIRECTORY_REPORTS/.selected"
rm -rf "$DIRECTORY_SELECTED"
mkdir -p "$DIRECTORY_SELECTED"
if [ "$#" -gt 0 ]; then
    for name_app in "$@"; do
        file_found=$(awk -F"$TAB" -v a="$name_app" '$1==a {print $2; exit}' "$FILE_FETCHED" 2>/dev/null || true)
        if [ -n "$file_found" ] && [ -f "$file_found" ]; then
            ln -sf "$file_found" "$DIRECTORY_SELECTED/$name_app.AppImage"
        else
            echo "corpus-run: not in the fetched corpus: $name_app" >&2
        fi
    done
else
    if [ -f "$FILE_FETCHED" ]; then
        while IFS="$TAB" read -r name_app path_file rest; do
            [ -f "$path_file" ] || continue
            ln -sf "$path_file" "$DIRECTORY_SELECTED/$name_app.AppImage"
        done < "$FILE_FETCHED"
    else
        for path_file in "$DIRECTORY_APPIMAGES"/*.AppImage "$DIRECTORY_APPIMAGES"/*.appimage; do
            [ -f "$path_file" ] || continue
            ln -sf "$path_file" "$DIRECTORY_SELECTED/$(basename "$path_file")"
        done
    fi
fi

count_apps=$(find "$DIRECTORY_SELECTED" -type l | wc -l)
[ "$count_apps" -gt 0 ] || fail "no AppImage to run; fetch the corpus first"
say "$count_apps application(s) to run"

# ---------------------------------------------------------------------------
# Field readers over the tools' own output.

field() { # <file> <key>
    sed -n "s/^$2: //p" "$1" | head -1
}

count_matching() { # <file> <extended-regex>
    grep -cE "$2" "$1" 2>/dev/null || true
}

FILE_SUMMARY="$DIRECTORY_REPORTS/summary.tsv"
printf 'app%sbytes%sdetection%scompression%sroot_entry%sroot_kind%susr_link%sterminal%sicons%swm_class%smime_types%ssignature%supdate_info%soutcome%swarnings%snotes\n' \
    "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" "$TAB" \
    > "$FILE_SUMMARY"
rm -f "$DIRECTORY_REPORTS/.installs"

for link_app in "$DIRECTORY_SELECTED"/*.AppImage; do
    [ -e "$link_app" ] || continue
    name_app=$(basename "$link_app" .AppImage)
    file_app=$(readlink -f "$link_app")
    DIRECTORY_APP_REPORT="$DIRECTORY_REPORTS/$name_app"
    rm -rf "$DIRECTORY_APP_REPORT"
    mkdir -p "$DIRECTORY_APP_REPORT"

    # A fresh tree for this application alone.
    DIRECTORY_SANDBOX="$DIRECTORY_WORK/sandbox/$name_app"
    rm -rf "$DIRECTORY_SANDBOX"
    mkdir -p "$DIRECTORY_SANDBOX/home/.local/share" "$DIRECTORY_SANDBOX/home/.config" \
        "$DIRECTORY_SANDBOX/share" "$DIRECTORY_SANDBOX/etc"
    export HOME="$DIRECTORY_SANDBOX/home"
    export XDG_DATA_HOME="$DIRECTORY_SANDBOX/home/.local/share"
    export XDG_DATA_DIRS="$DIRECTORY_SANDBOX/share"
    export XDG_CONFIG_HOME="$DIRECTORY_SANDBOX/home/.config"
    export XDG_CONFIG_DIRS="$DIRECTORY_SANDBOX/etc"
    unset DISPLAY WAYLAND_DISPLAY || true

    status_inspect=0
    "$INSPECT" "$file_app" > "$DIRECTORY_APP_REPORT/inspect.txt" 2>&1 || status_inspect=$?

    status_plan=0
    "$TOOL" plan "$file_app" > "$DIRECTORY_APP_REPORT/plan.txt" 2>&1 || status_plan=$?

    status_install=skip
    if [ "${SKIP_INSTALL:-0}" != "1" ] && [ "$status_plan" -eq 0 ]; then
        if "$TOOL" install --yes --no-move "$file_app" \
            > "$DIRECTORY_APP_REPORT/install.txt" 2>&1; then
            status_install=ok
        else
            status_install=failed
        fi
        "$TOOL" list > "$DIRECTORY_APP_REPORT/list.txt" 2>&1 || true
    fi

    # ------------------------------------------------------------------
    # What the application is.

    detection=$(field "$DIRECTORY_APP_REPORT/inspect.txt" detection)
    compression=$(field "$DIRECTORY_APP_REPORT/inspect.txt" 'squashfs-compression')
    root_entry=$(field "$DIRECTORY_APP_REPORT/inspect.txt" 'embedded-desktop')
    signature=$(field "$DIRECTORY_APP_REPORT/inspect.txt" signature)
    update_info=$(field "$DIRECTORY_APP_REPORT/inspect.txt" 'update-information')
    bytes=$(wc -c < "$file_app" | tr -d ' ')

    # Which of the three shapes the root desktop entry takes: a real file, a link into
    # the payload (Audacity's shape), or nothing at all.
    root_kind=none
    if grep -qE "^  symlink [0-9]+ /[^/]*\.desktop$" "$DIRECTORY_APP_REPORT/inspect.txt"; then
        root_kind=symlink
    elif grep -qE "^  file [0-9]+ /[^/]*\.desktop$" "$DIRECTORY_APP_REPORT/inspect.txt"; then
        root_kind=file
    fi
    usr_link=no
    if grep -qE "^  symlink [0-9]+ /usr$" "$DIRECTORY_APP_REPORT/inspect.txt"; then
        usr_link=yes
    fi
    terminal=no
    if grep -qxF 'Terminal=true' "$DIRECTORY_APP_REPORT/inspect.txt"; then
        terminal=yes
    fi

    # ------------------------------------------------------------------
    # What the tools made of it.

    outcome=ok
    if [ "$status_plan" -ne 0 ]; then
        outcome="error: $(sed -n 's/^error: //p' "$DIRECTORY_APP_REPORT/plan.txt" | head -1)"
    elif [ "$status_inspect" -ne 0 ]; then
        outcome='error: inspect failed'
    fi

    icons=$(count_matching "$DIRECTORY_APP_REPORT/plan.txt" '^  ([0-9]+x[0-9]+|scalable) \.(png|svg|svgz|xpm) ')
    warnings=$(count_matching "$DIRECTORY_APP_REPORT/plan.txt" '^warning: ')
    notes=$(sed -n 's/^warning: //p' "$DIRECTORY_APP_REPORT/plan.txt" | head -2 | tr '\n' ';' | tr -d '\t')
    wm_class=$(field "$DIRECTORY_APP_REPORT/plan.txt" 'startup-wm-class')
    mime_types=$(field "$DIRECTORY_APP_REPORT/plan.txt" 'mime-types')

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$name_app" "$bytes" "$detection" "$compression" "$root_entry" "$root_kind" \
        "$usr_link" "$terminal" "$icons" "$wm_class" "$mime_types" "$signature" \
        "$update_info" "$outcome" "$warnings" "$notes" >> "$FILE_SUMMARY"

    if [ "$status_install" != "skip" ]; then
        printf '%s\tinstall=%s\n' "$name_app" "$status_install" >> "$DIRECTORY_REPORTS/.installs"
    fi

    printf '%-22s %-8s %-16s root=%-7s usr=%-3s icons=%-3s warn=%-2s %s\n' \
        "$name_app" "$detection" "$compression" "$root_kind" "$usr_link" "$icons" \
        "$warnings" "$outcome"

    [ "${KEEP_SANDBOX:-0}" = "1" ] || rm -rf "$DIRECTORY_SANDBOX"
done

# ---------------------------------------------------------------------------
# The catalogue: what the corpus found, counted.

FILE_EDGES="$DIRECTORY_REPORTS/edge-cases.txt"
{
    echo "corpus edge cases"
    echo "generated $(date -Iseconds)"
    echo
    echo "== outcomes =="
    awk -F"$TAB" 'NR>1 {print $14}' "$FILE_SUMMARY" | sort | uniq -c | sort -rn
    echo
    echo "== warnings, by text =="
    for file_plan in "$DIRECTORY_REPORTS"/*/plan.txt; do
        [ -f "$file_plan" ] || continue
        sed -n 's/^warning: //p' "$file_plan"
    done | sort | uniq -c | sort -rn
    echo
    echo "== shapes =="
    for column_name in root_kind usr_link terminal; do
        printf '%s:\n' "$column_name"
        awk -F"$TAB" -v c="$column_name" '
            NR==1 { for (i=1;i<=NF;i++) if ($i==c) k=i; next }
            { print $k }' "$FILE_SUMMARY" | sort | uniq -c
    done
    printf 'no icons, among the applications that planned cleanly:\n'
    awk -F"$TAB" 'NR>1 && $9==0 && $14=="ok" {print "  "$1}' "$FILE_SUMMARY"
    echo "== install stage =="
    if [ -f "$DIRECTORY_REPORTS/.installs" ]; then
        sort "$DIRECTORY_REPORTS/.installs"
    else
        echo "  (not run)"
    fi
} > "$FILE_EDGES"

say "summary:    $FILE_SUMMARY"
say "edge cases: $FILE_EDGES"
sed -n '1,40p' "$FILE_EDGES"
