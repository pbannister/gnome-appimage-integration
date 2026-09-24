#!/bin/sh
#
# release-package.sh: turn a built tree into the files a GitHub release publishes.
#
# Output, in dataflow.out/release/:
#   gnome-appimage-integration-linux-<arch>.tar.gz   the products, with bin/ and share/
#   SHA256SUMS                                       the digest of that tarball
#   RELEASE-NOTES.md                                 what the release says
#
# Packing one build is deterministic: sorted names, no owner, and the commit's own
# timestamp, so packing twice produces one digest.  (Building again does not: the build
# counter the version reports changes with every build.)  scripts/install.sh expects
# exactly these names, which is why they carry no version: GitHub serves
#   https://github.com/<repository>/releases/latest/download/<asset>
# without an API call, and a version can still be pinned by adding the tag to the path.
set -eu

DIRECTORY_SCRIPT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPOSITORY_ROOT=$(CDPATH= cd -- "$DIRECTORY_SCRIPT/.." && pwd)
DIRECTORY_BUILD="$REPOSITORY_ROOT/dataflow.out/build"
DIRECTORY_RELEASE="$REPOSITORY_ROOT/dataflow.out/release"
DIRECTORY_STAGE="$DIRECTORY_RELEASE/stage"
PROGRAM_NAME="gnome-appimage-integration"

if [ ! -x "$DIRECTORY_BUILD/appimage-integrate" ]; then
    echo "release-package: there is no build in $DIRECTORY_BUILD" >&2
    echo "release-package: run 'make build' first" >&2
    exit 1
fi

case "$(uname -m)" in
    x86_64 | amd64)
        ARCHITECTURE=x86_64
        ;;
    aarch64 | arm64)
        ARCHITECTURE=aarch64
        ;;
    *)
        echo "release-package: no release is built for machine $(uname -m)" >&2
        exit 1
        ;;
esac

# The version the products report, which the build stamped from the commit.
VERSION=$("$DIRECTORY_BUILD/appimage-integrate" --version | awk '{print $2}')
COMMIT=$(git -C "$REPOSITORY_ROOT" rev-parse --short HEAD 2>/dev/null || echo "")
case "$VERSION" in
    *"$COMMIT"*)
        ;;
    *)
        echo "release-package: warning: the built products report $VERSION," >&2
        echo "release-package: warning: which does not name the current commit $COMMIT" >&2
        echo "release-package: warning: run 'make build' to package what this commit builds" >&2
        ;;
esac
FILE_TARBALL="$DIRECTORY_RELEASE/$PROGRAM_NAME-linux-$ARCHITECTURE.tar.gz"

rm -rf "$DIRECTORY_RELEASE"
mkdir -p "$DIRECTORY_STAGE/bin" "$DIRECTORY_STAGE/share/icons/hicolor/scalable/apps"

for program_name in appimage-inspect desktop-inspect appimage-integrate; do
    if [ ! -x "$DIRECTORY_BUILD/$program_name" ]; then
        echo "release-package: $DIRECTORY_BUILD/$program_name is missing" >&2
        exit 1
    fi
    cp "$DIRECTORY_BUILD/$program_name" "$DIRECTORY_STAGE/bin/$program_name"
done

# The graphical activator is built only where the GTK4 development files are present;
# the release notes say whether this build has it.
HAS_ACTIVATOR=0
if [ -x "$DIRECTORY_BUILD/appimage-activator" ]; then
    cp "$DIRECTORY_BUILD/appimage-activator" "$DIRECTORY_STAGE/bin/appimage-activator"
    HAS_ACTIVATOR=1
fi

FILE_ICON="$DIRECTORY_BUILD/icons/appimage-activator.svg"
if [ ! -f "$FILE_ICON" ]; then
    FILE_ICON="$REPOSITORY_ROOT/sources/tools/icons/appimage-activator.svg"
fi
if [ -f "$FILE_ICON" ]; then
    cp "$FILE_ICON" "$DIRECTORY_STAGE/share/icons/hicolor/scalable/apps/appimage-activator.svg"
fi

cp "$REPOSITORY_ROOT/README.md" "$DIRECTORY_STAGE/README.md"
printf '%s\n' "$VERSION" > "$DIRECTORY_STAGE/VERSION"

# One timestamp for every member, taken from the commit, and no gzip timestamp or
# original name, so the same commit always packs to the same bytes.
FILE_MTIME=$(git -C "$REPOSITORY_ROOT" log -1 --format=%ct 2>/dev/null || date +%s)
tar --create --file - \
    --directory "$DIRECTORY_STAGE" \
    --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$FILE_MTIME" \
    . | gzip -9n > "$FILE_TARBALL"

(
    cd "$DIRECTORY_RELEASE"
    sha256sum "$(basename "$FILE_TARBALL")" > SHA256SUMS
)
# The publisher reads the version from here, so it does not have to unpack anything.
printf '%s\n' "$VERSION" > "$DIRECTORY_RELEASE/VERSION"

if [ 1 -eq "$HAS_ACTIVATOR" ]; then
    ACTIVATOR_LINE="The graphical activator (appimage-activator) is included; it needs GTK4 at run time."
else
    ACTIVATOR_LINE="The graphical activator is not included in this build (no GTK4 development files were present); the tools fall back to zenity, then to printed instructions."
fi

cat > "$DIRECTORY_RELEASE/RELEASE-NOTES.md" <<NOTES
# gnome-appimage-integration $VERSION

Reads AppImages, writes proper desktop integration for them, and manages the launchers
afterwards, on GNOME.  Built for linux-$ARCHITECTURE.

## Install

\`\`\`sh
curl -fsSL https://raw.githubusercontent.com/pbannister/gnome-appimage-integration/master/scripts/install.sh | sh
\`\`\`

The script takes the tarball from this release, checks it against \`SHA256SUMS\`, and
installs into \`\$HOME/.local\`.  Set \`PREFIX\` to install elsewhere, or \`VERSION\` to a tag
to pin one.  Then, if you want \`*.AppImage\` files to open with the AppImage Activator:

\`\`\`sh
appimage-integrate handler install
\`\`\`

## What is in the tarball

| Path | What it is |
| ---- | ---------- |
| \`bin/appimage-inspect\` | container facts, the embedded desktop entry, and the update information |
| \`bin/desktop-inspect\` | desktop entry facts, icon resolution, MIME ownership, and why |
| \`bin/appimage-integrate\` | plan, install, uninstall, list, refresh, migrate, update, windows, audit |
| \`bin/appimage-activator\` | the GTK4 window the launchers and the handler open |
| \`share/icons/hicolor/scalable/apps/appimage-activator.svg\` | the activator icon |
| \`VERSION\` | the version these binaries report |

$ACTIVATOR_LINE

## Testing the download

\`\`\`sh
sha256sum -c SHA256SUMS
\`\`\`
NOTES

rm -rf "$DIRECTORY_STAGE"

echo "release-package: $(basename "$FILE_TARBALL")"
echo "release-package: SHA256SUMS"
echo "release-package: RELEASE-NOTES.md"
echo "release-package: version $VERSION, linux-$ARCHITECTURE"
