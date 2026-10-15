#!/bin/sh
#
# Tool-gated test: the corpus runner.
#
# scripts/corpus-run.sh runs the tools against a corpus of AppImages, giving each one a
# throwaway HOME and XDG tree so nothing is inherited from an earlier application, an
# earlier run, or the real desktop.  This test checks the catalogue it produces and, by
# running it twice, that the second run differs in nothing.
#
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
    pass_test "corpus runner (no mksquashfs)"
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
    pass_test "corpus runner (no host ELF)"
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_CORPUS="$DIRECTORY_TEMP/corpus"
mkdir -p "$DIRECTORY_CORPUS/appimages"

build_image() { # <name> <payload-dir>
    mksquashfs "$2" "$DIRECTORY_TEMP/$1.squashfs" -comp gzip -noappend -quiet >/dev/null
    cp "$FILE_ELF" "$DIRECTORY_CORPUS/appimages/$1.AppImage"
    printf '\101\111\002' | dd of="$DIRECTORY_CORPUS/appimages/$1.AppImage" bs=1 seek=8 \
        conv=notrunc 2>/dev/null
    size_now=$(wc -c < "$DIRECTORY_CORPUS/appimages/$1.AppImage")
    dd if=/dev/zero bs=1 count=$((4096 - size_now)) \
        >> "$DIRECTORY_CORPUS/appimages/$1.AppImage" 2>/dev/null || true
    cat "$DIRECTORY_TEMP/$1.squashfs" >> "$DIRECTORY_CORPUS/appimages/$1.AppImage"
    rm -f "$DIRECTORY_TEMP/$1.squashfs"
}

# A clean integration.
mkdir -p "$DIRECTORY_TEMP/p1/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_TEMP/p1/usr/share/icons/hicolor/48x48/apps/good.png"
printf 'diricon' > "$DIRECTORY_TEMP/p1/.DirIcon"
printf '[Desktop Entry]\nType=Application\nName=Good App\nExec=good %%F\nIcon=good\nStartupWMClass=GoodApp\n' \
    > "$DIRECTORY_TEMP/p1/good.desktop"
build_image Good "$DIRECTORY_TEMP/p1"

# The Audacity shape: the root desktop entry is a link.
mkdir -p "$DIRECTORY_TEMP/p2/share/applications"
printf '[Desktop Entry]\nType=Application\nName=Linked App\nExec=linked %%U\nIcon=linked\n' \
    > "$DIRECTORY_TEMP/p2/share/applications/org.example.Linked.desktop"
ln -s share/applications/org.example.Linked.desktop "$DIRECTORY_TEMP/p2/org.example.Linked.desktop"
build_image Linked "$DIRECTORY_TEMP/p2"

# No root desktop entry at all.
mkdir -p "$DIRECTORY_TEMP/p3/usr/share/applications"
printf '[Desktop Entry]\nType=Application\nName=NoRoot\nExec=noroot %%F\n' \
    > "$DIRECTORY_TEMP/p3/usr/share/applications/noroot.desktop"
build_image NoRoot "$DIRECTORY_TEMP/p3"

# A file name with spaces, which the launcher has to quote: this is what the NETGEAR
# AppImage in the real corpus is, and a first-word check that ignores the quoting
# reports its launcher as broken.
mkdir -p "$DIRECTORY_TEMP/p4/usr/share/icons/hicolor/48x48/apps"
printf 'png' > "$DIRECTORY_TEMP/p4/usr/share/icons/hicolor/48x48/apps/spaced.png"
printf '[Desktop Entry]\nType=Application\nName=Spaced App\nExec=spaced %%F\nIcon=spaced\n' \
    > "$DIRECTORY_TEMP/p4/spaced.desktop"
build_image "Spaced Image" "$DIRECTORY_TEMP/p4"

run_corpus() {
    env APPIMAGE_CORPUS_DIR="$DIRECTORY_CORPUS" \
        APPIMAGE_SANDBOX_DIR="$DIRECTORY_TEMP/sandbox" \
        APPIMAGE_INTEGRATE="$DIRECTORY_BUILD/appimage-integrate" \
        APPIMAGE_INSPECT="$DIRECTORY_BUILD/appimage-inspect" \
        sh "$REPOSITORY_ROOT/scripts/corpus-run.sh"
}

echo "=== the run catalogues every shape ==="
run_corpus > "$DIRECTORY_TEMP/run-one.txt" 2>&1 \
    || fail_test "the corpus run failed: $(cat "$DIRECTORY_TEMP/run-one.txt")"
FILE_SUMMARY="$DIRECTORY_CORPUS/reports/summary.tsv"
[ -f "$FILE_SUMMARY" ] || fail_test "no summary was written"

row_of() { # <app>
    awk -F"$(printf '\t')" -v a="$1" 'NR>1 && $1==a' "$FILE_SUMMARY"
}
if [ "$(grep -vc '^app' "$FILE_SUMMARY")" != "4" ]; then
    fail_test "the summary does not have four rows: $(cat "$FILE_SUMMARY")"
fi
printf '%s' "$(row_of Good)" | grep -q "file" \
    || fail_test "the clean AppImage was not recorded as a root file: $(row_of Good)"
printf '%s' "$(row_of Linked)" | grep -q "symlink" \
    || fail_test "the symlinked root entry was not detected: $(row_of Linked)"
printf '%s' "$(row_of NoRoot)" | grep -q "no root desktop entry" \
    || fail_test "the AppImage with no root entry did not report it: $(row_of NoRoot)"
if [ "$(row_of Good | awk -F"$(printf '\t')" '{print $9}')" -lt 1 ]; then
    fail_test "the clean AppImage was recorded with no icons: $(row_of Good)"
fi
if [ "$(row_of Good | awk -F"$(printf '\t')" '{print $16}')" = "failed" ]; then
    fail_test "the clean AppImage did not install"
fi
# The launcher each install wrote is checked, not just the install's exit status: the
# file, its Exec target, and the icon it names.
grep -q "$(printf 'verify\tappstream\toptout')$" "$FILE_SUMMARY" \
    || fail_test "the summary has no verify, appstream and optout columns"
# None of the synthetic images ships AppStream metadata, and their sandbox sets no
# opt-out condition, so both new columns read zero here: tests/72 and tests/71 cover
# what they count.
if [ "$(row_of Good | awk -F"$(printf '\t')" '{print $18}')" != "0" ] \
    || [ "$(row_of Good | awk -F"$(printf '\t')" '{print $19}')" != "0" ]; then
    fail_test "the appstream and optout columns are not zero for an AppImage with neither: $(row_of Good)"
fi
if [ "$(row_of Good | awk -F"$(printf '\t')" '{print $17}')" != "verified" ]; then
    fail_test "the clean AppImage's launcher was not verified: $(row_of Good)"
fi
if [ "$(row_of NoRoot | awk -F"$(printf '\t')" '{print $17}')" != "-" ]; then
    fail_test "an AppImage that never installed was reported as verified: $(row_of NoRoot)"
fi
# A launcher for a file whose name has spaces must verify: its Exec is quoted, and a
# check that splits on the first space would call it broken.
if [ "$(row_of "Spaced Image" | awk -F"$(printf '\t')" '{print $17}')" != "verified" ]; then
    fail_test "the launcher for a name with spaces was not verified: $(row_of "Spaced Image")"
fi

FILE_EDGES="$DIRECTORY_CORPUS/reports/edge-cases.txt"
for text_expected in "the AppImage has no root desktop entry" "root_kind:" "symlink" \
    "== launcher verification =="; do
    grep -q "$text_expected" "$FILE_EDGES" \
        || fail_test "the edge-case catalogue does not mention '$text_expected'"
done

echo "=== each application was given its own sandbox, and it was cleaned up ==="
if [ -d "$DIRECTORY_TEMP/sandbox" ]; then
    leftovers=$(find "$DIRECTORY_TEMP/sandbox" -mindepth 1 -maxdepth 1 | wc -l)
    if [ "$leftovers" != "0" ]; then
        fail_test "$leftovers sandbox directory(ies) were left behind"
    fi
fi

echo "=== a second run inherits nothing from the first ==="
cp "$FILE_SUMMARY" "$DIRECTORY_TEMP/summary-one.tsv"
run_corpus > "$DIRECTORY_TEMP/run-two.txt" 2>&1 \
    || fail_test "the second corpus run failed: $(cat "$DIRECTORY_TEMP/run-two.txt")"
if ! diff -u "$DIRECTORY_TEMP/summary-one.tsv" "$FILE_SUMMARY" > "$DIRECTORY_TEMP/diff.txt"; then
    fail_test "the second run produced a different catalogue: $(cat "$DIRECTORY_TEMP/diff.txt")"
fi

pass_test "corpus runner"
