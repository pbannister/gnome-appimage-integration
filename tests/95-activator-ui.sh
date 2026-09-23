#!/bin/sh
#
# Portable test: the graphical activator is present and wired up, the handler
# registration installs its own icon, names itself AppImage Activator for the
# right-click menu, and migrates the pre-rename files away.  The window itself is
# driven headlessly under Xvfb, because the program is the only interface it has.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

FILE_ACTIVATOR="$DIRECTORY_BUILD/appimage-activator"
if [ ! -x "$FILE_ACTIVATOR" ]; then
    echo "SKIP: appimage-activator was not built (libgtk-4-dev is missing)"
    pass_test "graphical activator (not built)"
    exit 0
fi

if [ ! -f "$DIRECTORY_BUILD/icons/appimage-activator.svg" ]; then
    fail_test "the activator icon was not copied to the build tree"
fi

# The window's application id is the program name, because no Gtk.Application id is
# set: GTK uses the application id when there is one and the program name otherwise.
FILE_SOURCE="$REPOSITORY_ROOT/sources/tools/appimage_activator_ui.cpp"
grep -q 'g_set_prgname("appimage-activator")' "$FILE_SOURCE" \
    || fail_test "the activator does not set its program name"
grep -q 'g_set_application_name("AppImage Activator")' "$FILE_SOURCE" \
    || fail_test "the activator window is not named AppImage Activator"
if grep -q 'gtk_application_new("[^"]' "$FILE_SOURCE"; then
    fail_test "a Gtk.Application id would become the window application id"
fi
# A second launch must open its own window for its own AppImage.
grep -q 'G_APPLICATION_NON_UNIQUE' "$FILE_SOURCE" \
    || fail_test "the activator is not multi-instance"
# An existing integration must be reported, not silently replaced.
grep -q 'is already installed' "$FILE_SOURCE" \
    || fail_test "the activator does not report existing integrations"
# The tool must look for the program it launches.
grep -q 'handler_ui_program' "$REPOSITORY_ROOT/sources/tools/appimage_integrate.cpp" \
    || fail_test "the tool does not reference the graphical activator"
# The window shows three logs, and Status is the page it opens on.
for TAB_NAME in Status Discovered Actions; do
    grep -q "add_text_page(\"$TAB_NAME\"" "$FILE_SOURCE" \
        || fail_test "the window has no $TAB_NAME tab"
done
grep -q 'gtk_notebook_set_current_page(GTK_NOTEBOOK(p_notebook_), 0)' "$FILE_SOURCE" \
    || fail_test "the Status tab is not the page shown first"

# With no display the handler must fall back to printed instructions, never hang.
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$DIRECTORY_BUILD/appimage-integrate" handle "$DIRECTORY_TEMP/Missing.AppImage" \
    > "$DIRECTORY_TEMP/handle.txt" 2>&1 || true
grep -q 'appimage-integrate' "$DIRECTORY_TEMP/handle.txt" \
    || fail_test "the headless handler printed no instructions"

# Registering the handler is exercised in an isolated XDG home.
if ! command -v xdg-mime >/dev/null 2>&1; then
    echo "SKIP: xdg-mime is not available (tool-gated)"
    pass_test "graphical activator (program only)"
    exit 0
fi

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_DATA="$DIRECTORY_XDG/home/.local/share"
mkdir -p "$DIRECTORY_DATA/applications" "$DIRECTORY_DATA/mime/packages" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc" "$DIRECTORY_XDG/home/.config"
cat > "$DIRECTORY_DATA/mime/packages/appimage.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/vnd.appimage">
    <comment>AppImage application bundle (Type 2)</comment>
    <generic-icon name="application-x-executable"/>
    <glob pattern="*.AppImage"/>
  </mime-type>
</mime-info>
XML

run_in_sandbox() {
    env -u DISPLAY -u WAYLAND_DISPLAY \
        HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_DATA" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

FILE_ACTIVATOR_ENTRY="$DIRECTORY_DATA/applications/appimage-activator.desktop"
FILE_ACTIVATOR_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-activator.svg"

# The pre-rename entry, its icon, and its record are on disk before the install,
# and the pre-rename entry is the current default for every AppImage MIME type.
FILE_LEGACY_ENTRY="$DIRECTORY_DATA/applications/appimage-handler.desktop"
FILE_LEGACY_ICON="$DIRECTORY_DATA/icons/hicolor/scalable/apps/appimage-handler.svg"
FILE_LEGACY_MANIFEST="$DIRECTORY_DATA/gnome-appimage-integration/appimage-handler.manifest"
printf '[Desktop Entry]\nType=Application\nName=AppImage Handler\nExec=/bin/true handle %%f\n' \
    > "$FILE_LEGACY_ENTRY"
mkdir -p "$(dirname -- "$FILE_LEGACY_ICON")"
printf 'old-icon' > "$FILE_LEGACY_ICON"
mkdir -p "$(dirname -- "$FILE_LEGACY_MANIFEST")"
printf 'previous_default=application/vnd.appimage\tappimagelauncher.desktop\n' \
    > "$FILE_LEGACY_MANIFEST"
cat > "$DIRECTORY_XDG/home/.config/mimeapps.list" <<'LIST'
[Default Applications]
application/vnd.appimage=appimage-handler.desktop
application/x-appimage=appimage-handler.desktop
application/x-iso9660-appimage=appimage-handler.desktop
LIST

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler install > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$FILE_ACTIVATOR_ENTRY" ]; then
    fail_test "handler install did not write the activator entry"
fi
grep -q '^Name=AppImage Activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the right-click item is not named AppImage Activator"
grep -q '^Icon=appimage-activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the activator entry does not use the AppImage Activator icon"
grep -q '^StartupWMClass=appimage-activator$' "$FILE_ACTIVATOR_ENTRY" \
    || fail_test "the activator entry has no StartupWMClass, so the dock cannot match the window"

if [ ! -f "$FILE_ACTIVATOR_ICON" ]; then
    fail_test "handler install did not install the activator icon"
fi
grep -q 'appimage-activator' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "the AppImage MIME type does not use the activator icon"
if grep -q 'application-x-executable' "$DIRECTORY_DATA/mime/packages/appimage.xml"; then
    fail_test "the AppImage MIME type still uses the generic icon"
fi

# The rename must leave nothing behind that still answers for the MIME types.
if [ -f "$FILE_LEGACY_ENTRY" ]; then
    fail_test "handler install left the pre-rename entry in place"
fi
if [ -f "$FILE_LEGACY_ICON" ]; then
    fail_test "handler install left the pre-rename icon in place"
fi
if [ -f "$FILE_LEGACY_MANIFEST" ]; then
    fail_test "handler install left the pre-rename record in place"
fi
if [ ! -f "$DIRECTORY_DATA/gnome-appimage-integration/appimage-activator.manifest" ]; then
    fail_test "handler install did not write the activator record"
fi
# The re-created handler is now the default for every AppImage MIME type.
if [ "$(run_in_sandbox xdg-mime query default application/vnd.appimage)" != "appimage-activator.desktop" ]; then
    fail_test "the activator is not the default handler after the rename"
fi
grep -q 'pre-rename' "$DIRECTORY_TEMP/install.txt" \
    || fail_test "handler install did not report the files it migrated"
# The previous default is carried across the rename, so uninstall can restore it.
grep -q 'previous_default=application/vnd.appimage	appimagelauncher.desktop' \
    "$DIRECTORY_DATA/gnome-appimage-integration/appimage-activator.manifest" \
    || fail_test "the rename lost the recorded previous default"

# The program name must equal the launcher's file name, or GNOME cannot match the
# window to it and the dock shows a generic icon.
FILE_PROGRAM_NAME=$(sed -n 's/.*g_set_prgname("\([^"]*\)").*/\1/p' "$FILE_SOURCE" | head -1)
if [ "$FILE_PROGRAM_NAME.desktop" != "$(basename -- "$FILE_ACTIVATOR_ENTRY")" ]; then
    fail_test "the window application id and the launcher id disagree"
fi

run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" handler uninstall > "$DIRECTORY_TEMP/uninstall.txt" 2>&1
if [ -f "$FILE_ACTIVATOR_ENTRY" ]; then
    fail_test "handler uninstall left the activator entry"
fi
if [ -f "$FILE_ACTIVATOR_ICON" ]; then
    fail_test "handler uninstall left the activator icon"
fi
grep -q 'application-x-executable' "$DIRECTORY_DATA/mime/packages/appimage.xml" \
    || fail_test "handler uninstall did not restore the original MIME icon"

# The window is driven under a virtual display with a stub tool, so this checks the
# program's own behaviour rather than an integration.
if ! command -v xvfb-run >/dev/null 2>&1; then
    echo "SKIP: xvfb-run is not available (tool-gated)"
    pass_test "graphical activator (program only)"
    exit 0
fi

FILE_STUB="$DIRECTORY_TEMP/appimage-integrate-stub"
FILE_RECORD="$DIRECTORY_TEMP/stub-calls.txt"
FILE_FAKE_IMAGE="$DIRECTORY_TEMP/Probe.AppImage"
: > "$FILE_FAKE_IMAGE"
cat > "$FILE_STUB" <<'STUB'
#!/bin/sh
# Stand-in for appimage-integrate: record the call, and answer explain --json.
printf '%s|' "$@" >> "$STUB_RECORD"
printf '\n' >> "$STUB_RECORD"
if [ "$1" = "explain" ]; then
    # The mode is chosen by the caller, so one stub can describe either a conflict
    # or an AppImage that is already where it belongs.
    sed "s/@MODE@/${STUB_MODE:-another launcher already represents this application}/" <<'JSON'
{"path":"/tmp/Probe.AppImage","name":"Probe App","generic_name":"Probe Tool",
 "comment":"Probe comment","version":"9.9.10","version_source":"X-AppImage-Version",
 "mode":"@MODE@","valid":false,"file_size":1024,"installed":"",
 "error":"1 existing launcher(s) already represent this application:\n  /home/u/.local/share/applications/org.example.Probe-2.desktop  (this tool)\nchoose --replace to back them up and install this version in their place, or --add to install alongside them",
 "conflicts":[{"desktop_id":"org.example.Probe.desktop",
               "path":"/tmp/org.example.Probe.desktop","name":"Probe App",
               "origin":"this tool (upgrade)","upgrade":true,"repair":false,
               "exec_exists":true,"version":"9.9.9","appimage":"/tmp/Probe.AppImage"}]}
JSON
fi
exit 0
STUB
chmod 755 "$FILE_STUB"

# GDK_BACKEND is forced so the window can only appear on the virtual display: this
# host runs a Wayland session, and GTK would otherwise prefer it.
run_driven() {
    # STUB_MODE decides what the stub reports; the assignment on the command line
    # exports it, which a plain shell variable set for the function would not.
    STUB_MODE="${STUB_MODE:-another launcher already represents this application}" \
        STUB_RECORD="$FILE_RECORD" XDG_DATA_HOME="$DIRECTORY_TEMP/home/.local/share" \
        GDK_BACKEND=x11 xvfb-run -a "$FILE_ACTIVATOR" --tool "$FILE_STUB" "$@" "$FILE_FAKE_IMAGE"
}

# Integrate, then Add alongside with a typed name: the field is shown prefilled,
# and the typed value reaches the tool as --name.
if ! run_driven --set-name "Probe App 9.9.10" --activate integrate,add-alongside \
    > "$DIRECTORY_TEMP/driven.txt" 2>&1; then
    cat "$DIRECTORY_TEMP/driven.txt" >&2
    fail_test "the activator did not run under the virtual display"
fi
grep -Fq 'install|--yes|--add|--name|Probe App 9.9.10|' "$FILE_RECORD" \
    || fail_test "the typed name was not passed to install"

# The three logs must say what happened: the current status, the evidence it was
# deduced from, and the command that changed the system.
grep -q '^=== Status ===$' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the driven run printed no Status tab"
grep -q 'This application is already installed.' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Status tab does not carry the already-installed notice"
grep -q 'Existing launchers for this application:' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Discovered tab does not list the launchers it found"
# The tool's own report belongs in Status, not in a label above the buttons.
grep -q '1 existing launcher(s) already represent this application:' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Status tab does not carry the tool's report"
grep -q 'choose --replace to back them up and install this version in their place' \
    "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Status tab does not carry the tool's two choices"
grep -q 'Identifier' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Discovered tab does not report the identifier it computed"
grep -Fq 'install --yes --add --name "Probe App 9.9.10"' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the Actions tab does not log the install command"

# Without a typed name the field must be prefilled from the AppImage.
: > "$FILE_RECORD"
run_driven --activate integrate,add-alongside > /dev/null 2>&1
grep -Fq 'install|--yes|--add|--name|Probe App|' "$FILE_RECORD" \
    || fail_test "the Name field was not prefilled from the AppImage"

# An AppImage that is already where it belongs must say so, and stop offering to
# replace what is correct.
STUB_MODE="properly integrated" run_driven --activate back > "$DIRECTORY_TEMP/integrated.txt" 2>&1 \
    || fail_test "the activator did not run for a properly integrated AppImage"
grep -q '^State:     properly integrated$' "$DIRECTORY_TEMP/integrated.txt" \
    || fail_test "the Status tab does not report a properly integrated AppImage"
grep -q 'Nothing more to do' "$DIRECTORY_TEMP/integrated.txt" \
    || fail_test "the Status tab does not say that nothing is left to do"
if grep -q 'This application is already installed.' "$DIRECTORY_TEMP/integrated.txt"; then
    fail_test "a properly integrated AppImage still offers to replace what is correct"
fi

# The window's WM_CLASS must be the activator's own name, which is what GNOME
# matches the launcher against.
if command -v Xvfb >/dev/null 2>&1 && command -v xprop >/dev/null 2>&1 \
    && command -v xwininfo >/dev/null 2>&1; then
    Xvfb -displayfd 3 -screen 0 800x600x24 3> "$DIRECTORY_TEMP/display" > /dev/null 2>&1 &
    XVFB_PROCESS=$!
    sleep 2
    DISPLAY_NUMBER=$(tr -d '\n' < "$DIRECTORY_TEMP/display")
    if [ -n "$DISPLAY_NUMBER" ]; then
        STUB_RECORD="$FILE_RECORD" DISPLAY=":$DISPLAY_NUMBER" GDK_BACKEND=x11 \
            XDG_DATA_HOME="$DIRECTORY_TEMP/home/.local/share" \
            "$FILE_ACTIVATOR" --tool "$FILE_STUB" "$FILE_FAKE_IMAGE" > /dev/null 2>&1 &
        ACTIVATOR_PROCESS=$!
        sleep 3
        FOUND_CLASS=0
        for WINDOW_ID in $(DISPLAY=":$DISPLAY_NUMBER" xwininfo -root -children 2>/dev/null \
            | grep -oE '^ +0x[0-9a-f]+' | tr -d ' '); do
            WINDOW_CLASS=$(DISPLAY=":$DISPLAY_NUMBER" xprop -id "$WINDOW_ID" WM_CLASS 2>/dev/null \
                || true)
            case "$WINDOW_CLASS" in
                *'"appimage-activator", "appimage-activator"'*)
                    FOUND_CLASS=1
                    break
                    ;;
            esac
        done
        kill "$ACTIVATOR_PROCESS" 2>/dev/null || true
        if [ "$FOUND_CLASS" -ne 1 ]; then
            kill "$XVFB_PROCESS" 2>/dev/null || true
            fail_test "the window WM_CLASS is not appimage-activator, so the dock cannot match it"
        fi
    fi
    kill "$XVFB_PROCESS" 2>/dev/null || true
    wait "$XVFB_PROCESS" 2>/dev/null || true
fi

pass_test "graphical activator"
