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
if [ "$1" = "explain" ] && [ "$2" != "--json" ]; then
    # The human report Inspect appends to the Discovered tab, carrying the two lines
    # the window emphasises.
    printf 'AppImage: %s\ndetection: type-2\nsize: 1024 bytes\n\nthis run:     %s\ninstall preview unavailable: 1 existing launcher(s) already represent this application:\n' \
        "$2" "${STUB_MODE:-another launcher already represents this application}"
    exit 0
fi
if [ "$1" = "explain" ]; then
    # The story of this AppImage is chosen by the caller: its version, the installed
    # version, how the two compare, and how many launchers already exist.
    printf '{"path":"%s","name":"Probe App","generic_name":"Probe Tool","comment":"Probe comment","version":"%s","version_source":"X-AppImage-Version","mode":"%s","installed_version":"%s","version_relation":"%s","signature_mismatch":%s,"signature_stored":"%s","signature_computed":"%s","valid":false,"file_size":1024,"installed":"","error":"1 existing launcher(s) already represent this application:\\n  /home/u/.local/share/applications/org.example.Probe-2.desktop  (this tool)\\nchoose --replace to back them up and install this version in their place, or --add to install alongside them","conflicts":[' \
        "$3" "${STUB_VERSION:-9.9.10}" \
        "${STUB_MODE:-another launcher already represents this application}" \
        "${STUB_INSTALLED_VERSION:-9.9.9}" "${STUB_RELATION:-same}" \
        "${STUB_SIGNATURE_MISMATCH:-false}" "${STUB_SIGNATURE_STORED:-}" \
        "${STUB_SIGNATURE_COMPUTED:-}"
    CONFLICT_INDEX=1
    while [ "$CONFLICT_INDEX" -le "${STUB_CONFLICTS:-1}" ]; do
        if [ "$CONFLICT_INDEX" -gt 1 ]; then
            printf ','
        fi
        printf '{"desktop_id":"org.example.Probe%s.desktop","path":"/tmp/org.example.Probe%s.desktop","name":"Probe App","origin":"this tool (upgrade)","upgrade":true,"repair":false,"exec_exists":true,"version":"%s","appimage":"/tmp/Probe.AppImage"}' \
            "$CONFLICT_INDEX" "$CONFLICT_INDEX" "${STUB_INSTALLED_VERSION:-9.9.9}"
        CONFLICT_INDEX=$((CONFLICT_INDEX + 1))
    done
    printf ']}\n'
    exit 0
fi
exit 0
STUB
chmod 755 "$FILE_STUB"

# GDK_BACKEND is forced so the window can only appear on the virtual display: this
# host runs a Wayland session, and GTK would otherwise prefer it.
run_driven() {
    # The stub's story variables are named here so the test can set any of them for
    # one run; naming them on the command line is what exports them to the stub, which
    # a plain shell variable set for the function would not do.
    STUB_MODE="${STUB_MODE:-another launcher already represents this application}" \
        STUB_VERSION="${STUB_VERSION:-9.9.10}" \
        STUB_INSTALLED_VERSION="${STUB_INSTALLED_VERSION:-9.9.9}" \
        STUB_RELATION="${STUB_RELATION:-same}" \
        STUB_CONFLICTS="${STUB_CONFLICTS:-1}" \
        STUB_SIGNATURE_MISMATCH="${STUB_SIGNATURE_MISMATCH:-false}" \
        STUB_SIGNATURE_STORED="${STUB_SIGNATURE_STORED:-}" \
        STUB_SIGNATURE_COMPUTED="${STUB_SIGNATURE_COMPUTED:-}" \
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
# The conflict choice is ordered by likely use too, with the resolving choice
# suggested and Back last.
run_driven --activate integrate > "$DIRECTORY_TEMP/prompted.txt" 2>&1
grep -q '^=== buttons: Replace existing\* Add alongside Back ===$' "$DIRECTORY_TEMP/prompted.txt" \
    || fail_test "the conflict choice is not Replace existing, Add alongside, Back with Replace existing suggested"

# Only the two fields that decide what happens are emphasised, in both the composed
# facts and the report Inspect appends.
sed -n '/^=== highlighted ===$/,/^=== Status ===$/p' "$DIRECTORY_TEMP/driven.txt" \
    > "$DIRECTORY_TEMP/highlighted.txt"
grep -q '^  This run ' "$DIRECTORY_TEMP/highlighted.txt" \
    || fail_test "the This run field is not highlighted in the Discovered log"
grep -q '^  Error ' "$DIRECTORY_TEMP/highlighted.txt" \
    || fail_test "the Error field is not highlighted in the Discovered log"
if grep -q 'Detection\|Identifier\|Existing launchers' "$DIRECTORY_TEMP/highlighted.txt"; then
    fail_test "a field other than This run and Error is highlighted"
fi

# Integrate leaves the resulting state on screen; the log waits in Actions. The
# buttons become the next step, with Run now suggested.
grep -q '^=== showing: Status ===$' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "Integrate did not leave the Status tab showing"
grep -q '^=== buttons: Run now\* Inspect Close ===$' "$DIRECTORY_TEMP/driven.txt" \
    || fail_test "the post-integration buttons are not Run now, Inspect, Close with Run now suggested"
# Inspect leaves its report on screen.
run_driven --activate inspect > "$DIRECTORY_TEMP/inspected.txt" 2>&1
grep -q '^=== showing: Discovered ===$' "$DIRECTORY_TEMP/inspected.txt" \
    || fail_test "Inspect did not leave the Discovered tab showing"
grep -Eq '^[0-9]{2}:[0-9]{2}:[0-9]{2}  appimage-integrate explain ' "$DIRECTORY_TEMP/inspected.txt" \
    || fail_test "Inspect did not append its report to the Discovered tab"
sed -n '/^=== highlighted ===$/,/^=== Status ===$/p' "$DIRECTORY_TEMP/inspected.txt" \
    > "$DIRECTORY_TEMP/highlighted-report.txt"
grep -q 'install preview unavailable:' "$DIRECTORY_TEMP/highlighted-report.txt" \
    || fail_test "the appended report's error line is not highlighted"
grep -q '^this run:' "$DIRECTORY_TEMP/highlighted-report.txt" \
    || fail_test "the appended report's this run line is not highlighted"
# Run once leaves the log of what it started.
run_driven --activate run-once > "$DIRECTORY_TEMP/ran.txt" 2>&1
grep -q '^=== showing: Actions ===$' "$DIRECTORY_TEMP/ran.txt" \
    || fail_test "Run once did not leave the Actions tab showing"
grep -Eq '^[0-9]{2}:[0-9]{2}:[0-9]{2}  appimage-integrate run --detached ' "$DIRECTORY_TEMP/ran.txt" \
    || fail_test "Run once did not log the command in the Actions tab"

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
# A properly integrated AppImage is the "same version, same file" case: nothing to
# do, so Close is the likely button.
grep -q '^=== buttons: Integrate Run once Inspect Close\* ===$' "$DIRECTORY_TEMP/integrated.txt" \
    || fail_test "a complete integration does not suggest Close"

# The owner's story, read from the real widget row and Name field. The first view:
# Integrate unless there is nothing to do or this build is older than what is
# installed; the choice: Add alongside when this build is older or when more than one
# launcher already exists, and in those two cases the version joins the name.
STORY_MODE="another launcher already represents this application"
story_buttons() { # <relation> <conflicts> <action> -> the row as printed
    STUB_RELATION="$1" STUB_CONFLICTS="$2" run_driven --activate "$3" 2>/dev/null \
        | grep -m1 '^=== buttons:'
}
expect_row() { # <label> <expected words> <relation> <conflicts> <action>
    ROW=$(story_buttons "$3" "$4" "$5")
    if [ "$ROW" != "=== buttons:$2 ===" ]; then
        fail_test "$1: expected '=== buttons:$2 ===', got '$ROW'"
    fi
}

# 1. The same version and the same file: nothing to do.
STUB_MODE="properly integrated" STUB_RELATION=same run_driven --activate back \
    > "$DIRECTORY_TEMP/story-same-file.txt" 2>&1
grep -q '^=== buttons: Integrate Run once Inspect Close\* ===$' "$DIRECTORY_TEMP/story-same-file.txt" \
    || fail_test "same version and same file should suggest Close"
# A payload that does not match its recorded digest must say so, and say that
# integrating it needs the override.
STUB_SIGNATURE_MISMATCH=true STUB_SIGNATURE_STORED=aaaa STUB_SIGNATURE_COMPUTED=bbbb \
    run_driven --activate back > "$DIRECTORY_TEMP/story-signature.txt" 2>&1
grep -q 'The payload does not match the digest recorded in .sha256_sig.' \
    "$DIRECTORY_TEMP/story-signature.txt" \
    || fail_test "a signature mismatch is not visible in Status"
grep -q 'Integrate is refused unless --ignore-signature is given.' \
    "$DIRECTORY_TEMP/story-signature.txt" \
    || fail_test "a signature mismatch does not name the override"

# 2. The same version, a different file: integrate it.
expect_row "same version, different file" " Integrate* Run once Inspect Close" same 1 back
# 3. A newer version: integrate it.
expect_row "newer version" " Integrate* Run once Inspect Close" newer 1 back
# 4. An older version: leave the newer installation alone, and say so clearly.
STUB_RELATION=older STUB_INSTALLED_VERSION=26.3.0 run_driven --activate back \
    > "$DIRECTORY_TEMP/story-older.txt" 2>&1
grep -q '^=== buttons: Integrate Run once Inspect Close\* ===$' "$DIRECTORY_TEMP/story-older.txt" \
    || fail_test "an older version should suggest Close"
grep -q 'This AppImage is older than the installed version.' "$DIRECTORY_TEMP/story-older.txt" \
    || fail_test "an older version is not visible in Status"
grep -q 'Close is suggested: the installed version is newer.' "$DIRECTORY_TEMP/story-older.txt" \
    || fail_test "Status does not explain why Close is suggested"
# 5. Integrating an older version: keep the newer one, under a name with the version.
STUB_RELATION=older STUB_INSTALLED_VERSION=26.3.0 run_driven --activate integrate \
    > "$DIRECTORY_TEMP/story-older-choice.txt" 2>&1
grep -q '^=== buttons: Replace existing Add alongside\* Back ===$' "$DIRECTORY_TEMP/story-older-choice.txt" \
    || fail_test "integrating an older version should suggest Add alongside"
grep -q '^=== name field: Probe App 9.9.10 ===$' "$DIRECTORY_TEMP/story-older-choice.txt" \
    || fail_test "the older version was not appended to the name"
# 6. More than one launcher: the same treatment.
STUB_RELATION=same STUB_CONFLICTS=2 run_driven --activate integrate \
    > "$DIRECTORY_TEMP/story-many.txt" 2>&1
grep -q '^=== buttons: Replace existing Add alongside\* Back ===$' "$DIRECTORY_TEMP/story-many.txt" \
    || fail_test "more than one launcher should suggest Add alongside"
grep -q '^=== name field: Probe App 9.9.10 ===$' "$DIRECTORY_TEMP/story-many.txt" \
    || fail_test "the version was not appended to the name with several launchers"
# 7. A newer version with one launcher: replace it, and leave the name alone.
STUB_RELATION=newer STUB_CONFLICTS=1 run_driven --activate integrate \
    > "$DIRECTORY_TEMP/story-newer-choice.txt" 2>&1
grep -q '^=== buttons: Replace existing\* Add alongside Back ===$' "$DIRECTORY_TEMP/story-newer-choice.txt" \
    || fail_test "a newer version with one launcher should suggest Replace existing"
grep -q '^=== name field: Probe App ===$' "$DIRECTORY_TEMP/story-newer-choice.txt" \
    || fail_test "the name gained a version although nothing has to be told apart"
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
