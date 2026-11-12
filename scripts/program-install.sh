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

# Copy to a temporary name and rename it over the destination: writing in place fails
# with "Text file busy" while the program is running, and a rename leaves any running
# copy with the file it started from.
install_program() { # <source> <destination>
    file_temporary="$2.installing.$$"
    cp "$1" "$file_temporary"
    chmod 755 "$file_temporary"
    mv -f "$file_temporary" "$2"
    echo "program-install: $2"
}

for program_name in appimage-inspect desktop-inspect appimage-integrate; do
    if [ ! -x "$DIRECTORY_BUILD/$program_name" ]; then
        echo "program-install: missing $DIRECTORY_BUILD/$program_name; run 'make build' first" >&2
        exit 1
    fi
    install_program "$DIRECTORY_BUILD/$program_name" "$DIRECTORY_BIN/$program_name"
done

# The graphical activator sits next to the tool so the tool can exec it. It is a
# GTK4 program, built only where the GTK4 development files are present.
if [ -x "$DIRECTORY_BUILD/appimage-activator" ]; then
    install_program "$DIRECTORY_BUILD/appimage-activator" "$DIRECTORY_BIN/appimage-activator"
else
    echo "program-install: appimage-activator was not built; the handler will use zenity" >&2
fi
# The earlier Python dialogs must not linger and shadow the program.
rm -f "$DIRECTORY_BIN/appimage_activator_ui.py"
rm -f "$DIRECTORY_BIN/appimage_handler_ui.py"
rm -f "$DIRECTORY_BIN/icons/appimage-handler.svg"

# The activator icon, both next to the tool and in the icon theme.
DIRECTORY_ICON="$DIRECTORY_BUILD/icons"
if [ -f "$DIRECTORY_ICON/appimage-activator.svg" ]; then
    mkdir -p "$DIRECTORY_BIN/icons"
    cp "$DIRECTORY_ICON/appimage-activator.svg" "$DIRECTORY_BIN/icons/appimage-activator.svg"
    # Raster copies travel next to the tool too, so `handler install` can find
    # them the same way it finds the SVG.
    for size_icon in 16 24 32 48 64 128 256 512; do
        if [ -f "$DIRECTORY_ICON/appimage-activator-$size_icon.png" ]; then
            cp "$DIRECTORY_ICON/appimage-activator-$size_icon.png" \
                "$DIRECTORY_BIN/icons/appimage-activator-$size_icon.png"
        fi
    done
    DIRECTORY_THEME="$PREFIX/share/icons/hicolor/scalable/apps"
    mkdir -p "$DIRECTORY_THEME"
    cp "$DIRECTORY_ICON/appimage-activator.svg" "$DIRECTORY_THEME/appimage-activator.svg"
    chmod 644 "$DIRECTORY_THEME/appimage-activator.svg"
    # The pre-rename icon in the theme would keep answering for the old name.
    rm -f "$DIRECTORY_THEME/appimage-handler.svg"
    echo "program-install: $DIRECTORY_THEME/appimage-activator.svg"
    # Raster copies of the same mark, so the dock icon resolves even where the
    # shell does not rasterise the scalable SVG in the user theme.
    for size_icon in 16 24 32 48 64 128 256 512; do
        if [ -f "$DIRECTORY_ICON/appimage-activator-$size_icon.png" ]; then
            directory_theme_size="$PREFIX/share/icons/hicolor/${size_icon}x${size_icon}/apps"
            mkdir -p "$directory_theme_size"
            cp "$DIRECTORY_ICON/appimage-activator-$size_icon.png" \
                "$directory_theme_size/appimage-activator.png"
            chmod 644 "$directory_theme_size/appimage-activator.png"
            echo "program-install: $directory_theme_size/appimage-activator.png"
        fi
    done
    # A stale cache hides the icon from GTK, which trusts a cache that is not
    # older than the theme directory and then never rescans it.
    if command -v gtk4-update-icon-cache >/dev/null 2>&1; then
        gtk4-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
    elif command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
    fi
fi
