#!/bin/sh
#
# corpus-fetch.sh: download the AppImage corpus named in scripts/corpus-list.tsv.
#
# The corpus is large and deliberately outside the repository, so `make clean` cannot
# take it and no accidental commit can carry it.  A file already cached with the size the
# list expects is not fetched again, and a file of the same name in ~/Applications is
# copied rather than downloaded, so a machine that already runs the application pays
# nothing for it.
#
# Usage:
#   sh scripts/corpus-fetch.sh
#
# Environment:
#   APPIMAGE_CORPUS_DIR   where the corpus lives (default: /datasets/appimage-corpus
#                         when /datasets exists, else ~/appimage-corpus)
#   CORPUS_LIST           the list to fetch (default scripts/corpus-list.tsv)
#   LIMIT                 fetch only the first N rows, for a quick pass
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$DIRECTORY_SCRIPT/lib/corpus-paths.sh"
DIRECTORY_WORK=$(corpus_directory)
DIRECTORY_APPIMAGES="$DIRECTORY_WORK/appimages"
DIRECTORY_RECORD="$DIRECTORY_WORK/corpus"
FILE_LIST=${CORPUS_LIST:-$DIRECTORY_SCRIPT/corpus-list.tsv}
FILE_FETCHED="$DIRECTORY_RECORD/fetched.tsv"

say() {
    echo "corpus-fetch: $1"
}

fail() {
    echo "corpus-fetch: $1" >&2
    exit 1
}

[ -f "$FILE_LIST" ] || fail "no corpus list at $FILE_LIST"
command -v curl > /dev/null 2>&1 || fail "curl is needed"
mkdir -p "$DIRECTORY_APPIMAGES" "$DIRECTORY_RECORD"

size_of() { # <file>
    wc -c < "$1" | tr -d ' '
}

count_total=0
count_cached=0
count_seeded=0
count_fetched=0
count_failed=0
size_fetched=0

printf 'app\tpath\tbytes\tsha256\tsource\n' > "$FILE_FETCHED"

while IFS="$(printf '\t')" read -r app url size note; do
    case "$app" in
        '' | '#'*) continue ;;
    esac
    count_total=$((count_total + 1))
    if [ -n "${LIMIT:-}" ] && [ "$count_total" -gt "$LIMIT" ]; then
        break
    fi

    case "$url" in
        local:*)
            file_source="$HOME/${url#local:}"
            name_file=$(basename "$file_source")
            ;;
        *)
            file_source=""
            name_file=$(basename "${url%%\?*}")
            ;;
    esac
    file_target="$DIRECTORY_APPIMAGES/$name_file"

    # Already cached and the right size: nothing to do.
    if [ -f "$file_target" ] && [ "$(size_of "$file_target")" = "$size" ]; then
        count_cached=$((count_cached + 1))
        printf '%s\t%s\t%s\t%s\t%s\n' "$app" "$file_target" "$size" \
            "$(sha256sum "$file_target" | cut -d' ' -f1)" "cache" >> "$FILE_FETCHED"
        say "cached: $app"
        continue
    fi

    # A local: source is a file this machine already has, named relative to $HOME, so a
    # corpus can include what is already installed without downloading it again.
    if [ -n "$file_source" ]; then
        if [ ! -f "$file_source" ]; then
            count_failed=$((count_failed + 1))
            echo "corpus-fetch: $app: $file_source is not there" >&2
            continue
        fi
        cp "$file_source" "$file_target"
        count_seeded=$((count_seeded + 1))
        printf '%s\t%s\t%s\t%s\t%s\n' "$app" "$file_target" "$(size_of "$file_target")" \
            "$(sha256sum "$file_target" | cut -d' ' -f1)" "local" >> "$FILE_FETCHED"
        say "local: $app"
        continue
    fi

    # Already on this machine: copy it rather than download it again.
    if [ -f "$HOME/Applications/$name_file" ] \
        && [ "$(size_of "$HOME/Applications/$name_file")" = "$size" ]; then
        cp "$HOME/Applications/$name_file" "$file_target"
        count_seeded=$((count_seeded + 1))
        printf '%s\t%s\t%s\t%s\t%s\n' "$app" "$file_target" "$size" \
            "$(sha256sum "$file_target" | cut -d' ' -f1)" "seeded" >> "$FILE_FETCHED"
        say "seeded from ~/Applications: $app"
        continue
    fi

    say "fetching: $app  ($(awk -v s="$size" 'BEGIN{printf "%.1f MB", s/1000000}'))"
    if ! curl -fL --retry 3 --connect-timeout 30 --max-time 3600 \
        -o "$file_target.part" "$url"; then
        rm -f "$file_target.part"
        count_failed=$((count_failed + 1))
        echo "corpus-fetch: download failed: $app  $url" >&2
        continue
    fi
    mv -f "$file_target.part" "$file_target"

    size_actual=$(size_of "$file_target")
    if [ "$size_actual" != "$size" ]; then
        echo "corpus-fetch: $app is $size_actual bytes, the list says $size" >&2
    fi
    chmod 755 "$file_target"
    count_fetched=$((count_fetched + 1))
    size_fetched=$((size_fetched + size_actual))
    printf '%s\t%s\t%s\t%s\t%s\n' "$app" "$file_target" "$size_actual" \
        "$(sha256sum "$file_target" | cut -d' ' -f1)" "download" >> "$FILE_FETCHED"
done < "$FILE_LIST"

say "$count_total app(s): $count_cached cached, $count_seeded seeded, $count_fetched fetched, $count_failed failed"
say "$(awk -v s="$size_fetched" 'BEGIN{printf "%.2f", s/1000000000}') GB downloaded"
say "record: $FILE_FETCHED"
[ "$count_failed" -eq 0 ] || exit 1
