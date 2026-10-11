#!/bin/sh
#
# corpus-paths.sh: where the AppImage corpus and its scratch live.  Sourced, not executed.
#
# A corpus of AppImages is gigabytes, so it belongs on the machine's bulk storage rather
# than in the home directory that holds the operating system: /datasets when that
# directory exists, and ~/appimage-corpus otherwise.  A derived project that keeps its
# bulk storage elsewhere should change the one line below rather than the callers.
#
# The per-application sandbox is the opposite kind of data: created and destroyed for
# every application, and read and written constantly while it exists, so it goes on the
# fast local filesystem in the temporary directory -- not next to the samples on a disk
# that would spend the whole run seeking.
#
# Environment:
#   APPIMAGE_CORPUS_DIR    the samples, the fetch record, the reports, restore points
#   APPIMAGE_SANDBOX_DIR   the throwaway desktops a corpus run makes and removes

# The directory that holds collected samples, on bulk storage where this machine has one.
corpus_directory() {
    if [ -n "${APPIMAGE_CORPUS_DIR:-}" ]; then
        printf '%s' "$APPIMAGE_CORPUS_DIR"
    elif [ -d /datasets ]; then
        printf '%s' "/datasets/appimage-corpus"
    else
        printf '%s' "$HOME/appimage-corpus"
    fi
}

# The directory a corpus run makes its per-application desktops under and removes.
sandbox_directory() {
    if [ -n "${APPIMAGE_SANDBOX_DIR:-}" ]; then
        printf '%s' "$APPIMAGE_SANDBOX_DIR"
    else
        printf '%s' "${TMPDIR:-/tmp}/appimage-corpus-sandbox"
    fi
}
