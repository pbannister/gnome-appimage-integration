#!/bin/sh
#
# Tool-gated test: conflict detection, replace-with-backup, and add-alongside.
# Prerequisites: mksquashfs, od, and a 64-bit little-endian host ELF.
# Everything happens under a temporary $HOME; the real desktop is untouched.
set -eu

. "$(dirname -- "$0")/lib/test_helpers.sh"

if ! command -v mksquashfs >/dev/null 2>&1 || ! command -v od >/dev/null 2>&1; then
    echo "SKIP: mksquashfs or od is not available (tool-gated)"
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
    exit 0
fi

build_program

DIRECTORY_TEMP=$(mktemp -d)
trap 'rm -rf "$DIRECTORY_TEMP"' EXIT

DIRECTORY_PAYLOAD="$DIRECTORY_TEMP/payload"
mkdir -p "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png' > "$DIRECTORY_PAYLOAD/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon' > "$DIRECTORY_PAYLOAD/.DirIcon"
cat > "$DIRECTORY_PAYLOAD/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
GenericName=Probe Tool
Comment=Probe comment
X-AppImage-Version=9.9.9
Exec=probe %U
Icon=probe
Categories=Utility;
StartupWMClass=ProbeApp
ENTRY

build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD" "$DIRECTORY_TEMP/Probe.AppImage" gzip

DIRECTORY_XDG="$DIRECTORY_TEMP/xdg"
DIRECTORY_APPLICATIONS="$DIRECTORY_XDG/home/.local/share/applications"
mkdir -p "$DIRECTORY_APPLICATIONS" "$DIRECTORY_XDG/share" "$DIRECTORY_XDG/etc"
cp "$DIRECTORY_TEMP/Probe.AppImage" "$DIRECTORY_TEMP/work.AppImage"

run_in_sandbox() {
    env HOME="$DIRECTORY_XDG/home" \
        XDG_DATA_HOME="$DIRECTORY_XDG/home/.local/share" \
        XDG_DATA_DIRS="$DIRECTORY_XDG/share" \
        XDG_CONFIG_HOME="$DIRECTORY_XDG/home/.config" \
        XDG_CONFIG_DIRS="$DIRECTORY_XDG/etc" \
        "$@"
}

# A launcher that already represents "Probe App", written by some other tool.
printf '[Desktop Entry]\nType=Application\nName=Probe App (1)\nExec=%s %%U\nIcon=probe\n' \
    "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop"

echo "=== plan must refuse and offer choices ==="
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/plan.txt" 2>&1; then
    fail_test "plan succeeded although a conflicting launcher exists"
fi
grep -q 'already represent' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not name the conflict"
grep -q -- '--replace' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not offer --replace"
grep -q -- '--add' "$DIRECTORY_TEMP/plan.txt" || fail_test "plan did not offer --add"

echo "=== install --replace backs up the existing launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --replace --yes "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/install.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "replace did not install the new launcher"
fi
if [ -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "replace left the conflicting launcher in place"
fi
ls "$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/backup/" | grep -q 'legacy.Probe.desktop' \
    || fail_test "replace did not back up the conflicting launcher"

FILE_IDENTIFIER=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | awk '{print $1}')
FILE_MANIFEST="$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/$FILE_IDENTIFIER.manifest"
if grep -E '^icon=' "$FILE_MANIFEST" | grep -q '\.desktop$'; then
    fail_test "a .desktop file was installed as an icon"
fi
if grep -E '^icon=' "$FILE_MANIFEST" | grep -q '/0x0/'; then
    fail_test "an icon was installed into an invalid size directory"
fi
if [ ! -f "$FILE_MANIFEST" ]; then
    fail_test "install did not write a manifest"
fi

echo "=== uninstall restores the replaced launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$FILE_IDENTIFIER" > /dev/null
if [ ! -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "uninstall did not restore the replaced launcher"
fi
if [ -f "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" ]; then
    fail_test "uninstall left our launcher behind"
fi

echo "=== install --add keeps the existing launcher and adds a distinct one ==="
cp "$DIRECTORY_TEMP/Probe.AppImage" "$DIRECTORY_TEMP/work.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --add --yes "$DIRECTORY_TEMP/work.AppImage" > "$DIRECTORY_TEMP/add.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/legacy.Probe.desktop" ]; then
    fail_test "add removed the existing launcher"
fi
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe-2.desktop" ]; then
    fail_test "add did not create a distinct launcher"
fi

echo "=== an upgrade with --replace keeps exactly one launcher ==="
# A second version of the same application, with different content and version.
DIRECTORY_PAYLOAD_TWO="$DIRECTORY_TEMP/payload2"
mkdir -p "$DIRECTORY_PAYLOAD_TWO/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png-two' > "$DIRECTORY_PAYLOAD_TWO/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon-two' > "$DIRECTORY_PAYLOAD_TWO/.DirIcon"
cat > "$DIRECTORY_PAYLOAD_TWO/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
GenericName=Probe Tool
Comment=Probe comment
X-AppImage-Version=9.9.10
Exec=probe %U
Icon=probe
Categories=Utility;
ENTRY
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD_TWO" "$DIRECTORY_TEMP/work2.AppImage" gzip
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --replace --yes "$DIRECTORY_TEMP/work2.AppImage" > "$DIRECTORY_TEMP/upgrade.txt" 2>&1
COUNT_LAUNCHERS=$(ls -1 "$DIRECTORY_APPLICATIONS"/org.example.Probe*.desktop 2>/dev/null | wc -l)
if [ "$COUNT_LAUNCHERS" -ne 1 ]; then
    fail_test "upgrade left $COUNT_LAUNCHERS launchers instead of one"
fi
grep -q 'work2.AppImage' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "upgrade did not point the launcher at the new AppImage"
COUNT_ENTRIES=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | grep -c . || true)
if [ "$COUNT_ENTRIES" -ne 1 ]; then
    fail_test "upgrade left $COUNT_ENTRIES manifests instead of one"
fi
if grep -E '^icon=' "$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/"*.manifest | grep -q '\.desktop$'; then
    fail_test "an upgrade installed a .desktop file as an icon"
fi

echo "=== re-integrating is harmless and remembers the window class ==="
FILE_INSTALLED="$DIRECTORY_XDG/home/Applications/work2.AppImage"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --wm-class ProbeAppClass "$FILE_INSTALLED" > /dev/null 2>&1
grep -q '^StartupWMClass=ProbeAppClass$' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "the window class was not written"
grep -q '^startup_wm_class=ProbeAppClass$' "$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/"*.manifest \
    || fail_test "the window class was not remembered"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$FILE_INSTALLED" > /dev/null 2>&1
grep -q '^StartupWMClass=ProbeAppClass$' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "re-integrating lost the window class"
COUNT_LAUNCHERS=$(ls -1 "$DIRECTORY_APPLICATIONS"/org.example.Probe*.desktop | wc -l)
if [ "$COUNT_LAUNCHERS" -ne 1 ]; then
    fail_test "re-integrating left $COUNT_LAUNCHERS launchers"
fi
COUNT_ENTRIES=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | grep -c . || true)
if [ "$COUNT_ENTRIES" -ne 1 ]; then
    fail_test "re-integrating left $COUNT_ENTRIES manifests"
fi

echo "=== add alongside an existing integration creates a second launcher ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --add "$FILE_INSTALLED" > /dev/null 2>&1
COUNT_LAUNCHERS=$(ls -1 "$DIRECTORY_APPLICATIONS"/org.example.Probe*.desktop | wc -l)
if [ "$COUNT_LAUNCHERS" -ne 2 ]; then
    fail_test "add alongside an existing integration made $COUNT_LAUNCHERS launchers"
fi
COUNT_ENTRIES=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list | grep -c . || true)
if [ "$COUNT_ENTRIES" -ne 2 ]; then
    fail_test "add alongside left $COUNT_ENTRIES records instead of two"
fi
# The launcher that was kept must still be recorded as ours.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$FILE_INSTALLED" > "$DIRECTORY_TEMP/keep.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/keep.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
origins = sorted(conflict["origin"] for conflict in data["conflicts"])
assert origins == ["this tool", "this tool (upgrade)"], origins
upgrade = [c for c in data["conflicts"] if c["origin"] == "this tool (upgrade)"][0]
assert upgrade["desktop_id"] == "org.example.Probe.desktop", upgrade
assert upgrade["appimage"].endswith("work2.AppImage"), upgrade
assert upgrade["exec_exists"] is True, upgrade
assert upgrade["version"] == "9.9.10", upgrade
print("kept launcher provenance ok")
PYTHON

echo "=== a different AppImage for the same identifier must be an explicit choice ==="
DIRECTORY_PAYLOAD_THREE="$DIRECTORY_TEMP/payload3"
mkdir -p "$DIRECTORY_PAYLOAD_THREE/usr/share/icons/hicolor/48x48/apps"
printf 'fake-png-three' > "$DIRECTORY_PAYLOAD_THREE/usr/share/icons/hicolor/48x48/apps/probe.png"
printf 'fake-diricon-three' > "$DIRECTORY_PAYLOAD_THREE/.DirIcon"
cat > "$DIRECTORY_PAYLOAD_THREE/org.example.Probe.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Probe App
X-AppImage-Version=9.9.11
Exec=probe %U
Icon=probe
Categories=Utility;
ENTRY
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD_THREE" "$DIRECTORY_TEMP/work3.AppImage" gzip
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/work3.AppImage" > "$DIRECTORY_TEMP/takeover.txt" 2>&1; then
    fail_test "plan let a different AppImage take over the same identifier silently"
fi
grep -q 'different AppImage for the same identifier' "$DIRECTORY_TEMP/takeover.txt" \
    || fail_test "the same-identifier takeover was not named"
grep -q -- '--add' "$DIRECTORY_TEMP/takeover.txt" \
    || fail_test "the same-identifier takeover offered no choice"
# The mode names the situation from the owner's point of view, without calling it
# blocked: nothing is wrong, a choice is simply required.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$DIRECTORY_TEMP/work3.AppImage" \
    > "$DIRECTORY_TEMP/takeover.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/takeover.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["mode"] == "another launcher already represents this application", data["mode"]
print("mode wording ok")
PYTHON
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --add --yes "$DIRECTORY_TEMP/work3.AppImage" > /dev/null 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe-3.desktop" ]; then
    fail_test "add alongside a same-identifier AppImage made no distinct launcher"
fi

echo "=== --name renames the launcher without touching the embedded entry ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --add \
    --name "Probe App 9.9.10" "$FILE_INSTALLED" > "$DIRECTORY_TEMP/named.txt" 2>&1
if [ ! -f "$DIRECTORY_APPLICATIONS/org.example.Probe-4.desktop" ]; then
    fail_test "install --add --name made no fourth launcher"
fi
grep -q '^Name=Probe App 9.9.10$' "$DIRECTORY_APPLICATIONS/org.example.Probe-4.desktop" \
    || fail_test "--name did not rename the new launcher"
# The other launchers keep the author's name.
grep -q '^Name=Probe App$' "$DIRECTORY_APPLICATIONS/org.example.Probe.desktop" \
    || fail_test "--name changed a launcher that was not being written"
# The AppImage still reports the embedded name, not the launcher's.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$FILE_INSTALLED" \
    > "$DIRECTORY_TEMP/named.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/named.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["name"] == "Probe App", data["name"]
print("embedded name preserved ok")
PYTHON

echo "=== explain shows the embedded entry ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain "$DIRECTORY_XDG/home/Applications/work.AppImage" > "$DIRECTORY_TEMP/explain.txt" 2>&1 || true
grep -q 'embedded desktop entry' "$DIRECTORY_TEMP/explain.txt" || fail_test "explain did not show the embedded entry"
grep -q 'Name=Probe App' "$DIRECTORY_TEMP/explain.txt" || fail_test "explain did not show the application name"

echo "=== explain --json carries the fields the graphical handler renders ==="
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" explain --json "$DIRECTORY_XDG/home/Applications/work.AppImage" > "$DIRECTORY_TEMP/explain.json" 2>/dev/null || true
python3 - "$DIRECTORY_TEMP/explain.json" <<'PYTHON'
import json
import sys

data = json.load(open(sys.argv[1], encoding="utf-8"))
assert data["name"] == "Probe App", data["name"]
assert data["generic_name"] == "Probe Tool", data["generic_name"]
assert data["comment"] == "Probe comment", data["comment"]
assert data["version"] == "9.9.9", data["version"]
assert data["version_source"] == "X-AppImage-Version", data["version_source"]
assert isinstance(data["conflicts"], list)
print("json fields ok")
PYTHON

echo "=== every install says which of the four things it is doing ==="
# A second application, so these checks do not disturb the Probe launchers.
DIRECTORY_PAYLOAD_REPAIR="$DIRECTORY_TEMP/payload-repair"
mkdir -p "$DIRECTORY_PAYLOAD_REPAIR/usr/share/icons/hicolor/48x48/apps"
printf 'repair-png' > "$DIRECTORY_PAYLOAD_REPAIR/usr/share/icons/hicolor/48x48/apps/repair.png"
printf 'repair-diricon' > "$DIRECTORY_PAYLOAD_REPAIR/.DirIcon"
cat > "$DIRECTORY_PAYLOAD_REPAIR/org.example.Repair.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Repair App
X-AppImage-Version=1.0
Exec=repair %U
Icon=repair
Categories=Utility;
StartupWMClass=RepairApp
ENTRY
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD_REPAIR" "$DIRECTORY_TEMP/Repair.AppImage" gzip
FILE_REPAIR_LAUNCHER="$DIRECTORY_APPLICATIONS/org.example.Repair.desktop"
FILE_REPAIR_MANAGED="$DIRECTORY_XDG/home/Applications/Repair.AppImage"

# 1. A new AppImage.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$DIRECTORY_TEMP/Repair.AppImage" \
    > "$DIRECTORY_TEMP/mode-new.txt" 2>&1
grep -q '^this-run: new integration$' "$DIRECTORY_TEMP/mode-new.txt" \
    || fail_test "a first install is not labelled as a new integration"

# 2. The same file again is a complete integration: say so, rather than reporting
#    work that would only rewrite the same files. One plan must be printed exactly
#    once: the cache-refresh children fork, and their inherited stdout buffer must
#    not print the plan a second and third time.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes "$FILE_REPAIR_MANAGED" \
    > "$DIRECTORY_TEMP/mode-update.txt" 2>&1
grep -q '^this-run: properly integrated$' "$DIRECTORY_TEMP/mode-update.txt" \
    || fail_test "a complete integration is not reported as properly integrated"
grep -q '^note: already integrated:' "$DIRECTORY_TEMP/mode-update.txt" \
    || fail_test "the properly-integrated run did not say why nothing is left to do"
COUNT_PLANS=$(grep -c '^this-run:' "$DIRECTORY_TEMP/mode-update.txt" || true)
if [ "$COUNT_PLANS" -ne 1 ]; then
    fail_test "the plan was printed $COUNT_PLANS times instead of once"
fi

# 2b. A launcher that runs this file, but whose managed directory is not where the
#     AppImage is, is an update rather than a complete integration.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan \
    --install-dir "$DIRECTORY_XDG/home/Elsewhere" "$FILE_REPAIR_MANAGED" \
    > "$DIRECTORY_TEMP/mode-elsewhere.txt" 2>&1
grep -q '^this-run: update the launcher in place$' "$DIRECTORY_TEMP/mode-elsewhere.txt" \
    || fail_test "an AppImage outside its managed directory is not labelled as an update"

# 3. The whole managed directory is removed, as an owner might do, taking the
#    AppImage with it.
mkdir -p "$DIRECTORY_XDG/home/Downloads"
mv "$FILE_REPAIR_MANAGED" "$DIRECTORY_XDG/home/Downloads/Repair.AppImage"
rm -rf "$DIRECTORY_XDG/home/Applications"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list > "$DIRECTORY_TEMP/list-missing.txt" 2>&1
grep 'org.example.Repair.desktop' "$DIRECTORY_TEMP/list-missing.txt" | grep -q '\[MISSING' \
    || fail_test "list did not mark the launcher whose AppImage is missing"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" audit > "$DIRECTORY_TEMP/audit-missing.txt" 2>&1 || true
grep -q 'Exec target is missing' "$DIRECTORY_TEMP/audit-missing.txt" \
    || fail_test "audit did not report the broken launcher"

# 4. Re-integrating from the new location repairs the launcher, and is not mistaken
#    for a different AppImage wanting the same identifier.
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes \
    "$DIRECTORY_XDG/home/Downloads/Repair.AppImage" > "$DIRECTORY_TEMP/mode-repair.txt" 2>&1
grep -q '^this-run: repair the launcher' "$DIRECTORY_TEMP/mode-repair.txt" \
    || fail_test "a moved AppImage is not labelled as a repair"
if [ ! -f "$FILE_REPAIR_MANAGED" ]; then
    fail_test "the repair did not recreate the managed directory and replace the AppImage"
fi
grep -q "Applications/Repair.AppImage %U" "$FILE_REPAIR_LAUNCHER" \
    || fail_test "the repair did not point the launcher at the managed path"
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list \
    | grep 'org.example.Repair.desktop' | grep -q '\[MISSING'; then
    fail_test "the repair left the record reporting a missing AppImage"
fi

# 5. A newer build of the same application is a replacement, and uninstall puts the
#    replaced launcher back.
DIRECTORY_PAYLOAD_REPAIR_TWO="$DIRECTORY_TEMP/payload-repair-two"
mkdir -p "$DIRECTORY_PAYLOAD_REPAIR_TWO/usr/share/icons/hicolor/48x48/apps"
printf 'repair-png-two' > "$DIRECTORY_PAYLOAD_REPAIR_TWO/usr/share/icons/hicolor/48x48/apps/repair.png"
printf 'repair-diricon-two' > "$DIRECTORY_PAYLOAD_REPAIR_TWO/.DirIcon"
sed 's/X-AppImage-Version=1.0/X-AppImage-Version=1.1/' \
    "$DIRECTORY_PAYLOAD_REPAIR/org.example.Repair.desktop" \
    > "$DIRECTORY_PAYLOAD_REPAIR_TWO/org.example.Repair.desktop"
build_synthetic_appimage "$FILE_ELF" "$DIRECTORY_PAYLOAD_REPAIR_TWO" \
    "$DIRECTORY_TEMP/Repair2.AppImage" gzip
if run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" plan "$DIRECTORY_TEMP/Repair2.AppImage" \
    > "$DIRECTORY_TEMP/mode-conflict.txt" 2>&1; then
    fail_test "a newer build was integrated without being asked how to treat the existing one"
fi
grep -q 'already represent this application' "$DIRECTORY_TEMP/mode-conflict.txt" \
    || fail_test "the newer build was not reported as a conflict"
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" install --yes --replace \
    "$DIRECTORY_TEMP/Repair2.AppImage" > "$DIRECTORY_TEMP/mode-replace.txt" 2>&1
grep -q '^this-run: replace an existing launcher$' "$DIRECTORY_TEMP/mode-replace.txt" \
    || fail_test "replacing a launcher is not labelled as a replacement"
grep -q 'Repair2.AppImage' "$FILE_REPAIR_LAUNCHER" \
    || fail_test "the replacement launcher does not run the newer AppImage"
ls "$DIRECTORY_XDG/home/.local/share/gnome-appimage-integration/backup/" | grep -q 'org.example.Repair.desktop' \
    || fail_test "the replaced launcher was not backed up"
FILE_REPAIR_IDENTIFIER=$(run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" list \
    | grep 'org.example.Repair.desktop' | awk '{print $1}')
run_in_sandbox "$DIRECTORY_BUILD/appimage-integrate" uninstall --identifier "$FILE_REPAIR_IDENTIFIER" \
    > /dev/null
grep -q 'Repair.AppImage' "$FILE_REPAIR_LAUNCHER" \
    || fail_test "uninstall did not restore the replaced launcher"

pass_test "integration conflicts"
