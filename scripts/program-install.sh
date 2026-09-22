#!/bin/sh
#
# program-install.sh: install the built readers and tools into a prefix.
# The default prefix is $HOME/.local, so no root access is required.
# The installed path is what the double-click handler and the launcher
# actions reference, so it must be stable.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"
PREFIX=${PREFIX:-$HOME/.local}
DIRECTORY_BIN="$PREFIX/bin"

mkdir -p "$DIRECTORY_BIN"

for program_name in appimage-inspect desktop-inspect appimage-integrate; do
    if [ ! -x "$DIRECTORY_BUILD/$program_name" ]; then
        echo "program-install: missing $DIRECTORY_BUILD/$program_name; run 'make build' first" >&2
        exit 1
    fi
    cp "$DIRECTORY_BUILD/$program_name" "$DIRECTORY_BIN/$program_name"
    chmod 755 "$DIRECTORY_BIN/$program_name"
    echo "program-install: $DIRECTORY_BIN/$program_name"
done
